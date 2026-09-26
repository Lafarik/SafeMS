#ifdef ENABLE_SAFEMS_KIOSK
#include "SafeMSKiosk.h"
#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <esp_http_server.h>
#include <SafeMSWebAssets.h>
#include "SafeMSFeed.h"
#include "SafeMSFeedSelfTest.h"
#include "SafeMSWebSocketProtocol.h"
#include <esp_timer.h>
#include <mbedtls/sha256.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <atomic>

namespace {
DNSServer dns;
httpd_handle_t server = nullptr;
bool ready = false;
volatile uint32_t lastMeshTick = 0;
const IPAddress address(192, 168, 4, 1);
QueueHandle_t incoming = nullptr;
SemaphoreHandle_t historyMutex = nullptr;
SafeMSFeed::History history;
std::atomic<bool> workPending{false};
std::atomic<uint32_t> accepted{0}, rejected{0}, dropped{0}, duplicates{0};
portMUX_TYPE clockMutex = portMUX_INITIALIZER_UNLOCKED;
int64_t clockOffsetMs = 0;
std::atomic<bool> clockSynced{false};

std::string snapshot() {
  xSemaphoreTake(historyMutex, portMAX_DELAY);
  auto body = SafeMSFeed::json(history.alerts, history.count);
  xSemaphoreGive(historyMutex);
  return body;
}

void sendSnapshot(void* argument) {
  const int fd = int(reinterpret_cast<intptr_t>(argument));
  if (httpd_ws_get_fd_info(server, fd) != HTTPD_WS_CLIENT_WEBSOCKET) return;
  const auto body = snapshot();
  httpd_ws_frame_t frame = {};
  frame.type = HTTPD_WS_TYPE_TEXT;
  frame.payload = reinterpret_cast<uint8_t*>(const_cast<char*>(body.data()));
  frame.len = body.size();
  if (httpd_ws_send_frame_async(server, fd, &frame) != ESP_OK) httpd_sess_trigger_close(server, fd);
}

void deliverMessages(void*) {
  SafeMSFeed::Alert alert;
  bool changed = false;
  // Bounded work per callback; further arrivals are picked up by the main loop.
  for (size_t i=0; i<SafeMSFeed::Capacity && xQueueReceive(incoming, &alert, 0)==pdTRUE; ++i) {
    xSemaphoreTake(historyMutex, portMAX_DELAY);
    const bool added = history.add(alert);
    xSemaphoreGive(historyMutex);
    if (added) { ++accepted; changed = true; } else ++duplicates;
  }
  if (changed) {
    size_t count = 7; int clients[7];
    if (httpd_get_client_list(server, &count, clients) == ESP_OK)
      for (size_t i=0; i<count; ++i) sendSnapshot(reinterpret_cast<void*>(intptr_t(clients[i])));
  }
  workPending.store(false);
}

esp_err_t messagesHandler(httpd_req_t* request) {
  const auto body = snapshot();
  httpd_resp_set_type(request, "application/json; charset=utf-8");
  httpd_resp_set_hdr(request, "Cache-Control", "no-store");
  return httpd_resp_send(request, body.data(), body.size());
}

esp_err_t websocketHandler(httpd_req_t* request) {
  if (request->method == HTTP_GET) {
    // The handshake completes after this handler returns. Send from queued work.
    return httpd_queue_work(server, sendSnapshot, reinterpret_cast<void*>(intptr_t(httpd_req_to_sockfd(request))));
  }
  httpd_ws_frame_t frame = {};
  if (httpd_ws_recv_frame(request, &frame, 0) != ESP_OK) return ESP_FAIL;
  // Read-only PWA requests and acknowledgements; no commands reach the mesh.
  if (frame.type != HTTPD_WS_TYPE_TEXT || !frame.final || !frame.len ||
      frame.len > SafeMSWebSocketProtocol::MaxPayload) return ESP_FAIL;
  uint8_t payload[SafeMSWebSocketProtocol::MaxPayload + 1] = {};
  frame.payload = payload;
  if (httpd_ws_recv_frame(request, &frame, SafeMSWebSocketProtocol::MaxPayload) != ESP_OK) return ESP_FAIL;
  const auto action = SafeMSWebSocketProtocol::parse(reinterpret_cast<const char*>(payload), frame.len);
  if (action == SafeMSWebSocketProtocol::Action::Snapshot) {
    sendSnapshot(reinterpret_cast<void*>(intptr_t(httpd_req_to_sockfd(request))));
    return ESP_OK;
  }
  // Do not answer ACKs: another snapshot would provoke another ACK from clients.
  // These confirmations are deliberately not stored, published, or sent by radio.
  return action == SafeMSWebSocketProtocol::Action::Acknowledgement ? ESP_OK : ESP_FAIL;
}

esp_err_t statusHandler(httpd_req_t* request) {
  char body[460];
  const uint32_t now = millis();
  const bool meshLoopRecent = lastMeshTick != 0 && uint32_t(now - lastMeshTick) < 1500;
  snprintf(body, sizeof(body),
    "{\"mode\":\"test\",\"uptime_s\":%lu,\"wifi_clients\":%u,\"free_heap\":%u,\"min_free_heap\":%u,\"mesh_loop_recent\":%s,\"pager_channel\":\"Krisenstab\",\"pager_accepted\":%u,\"pager_rejected\":%u,\"pager_dropped\":%u,\"pager_duplicates\":%u,\"pager_clock_synced\":%s}",
    (unsigned long)(now / 1000), WiFi.softAPgetStationNum(), ESP.getFreeHeap(),
    ESP.getMinFreeHeap(), meshLoopRecent ? "true" : "false", accepted.load(), rejected.load(), dropped.load(), duplicates.load(), clockSynced ? "true" : "false");
  httpd_resp_set_type(request, "application/json");
  httpd_resp_set_hdr(request, "Cache-Control", "no-store");
  return httpd_resp_send(request, body, HTTPD_RESP_USE_STRLEN);
}

esp_err_t pageHandler(httpd_req_t* request) {
  char path[256];
  const size_t length = strcspn(request->uri, "?");
  if (length >= sizeof(path)) return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Path too long");
  memcpy(path, request->uri, length); path[length] = 0;
  // Decode only valid escapes; lookup is against the fixed asset table, never a filesystem.
  size_t write = 0;
  for (size_t read = 0; read < length; ++read) {
    if (path[read] == '%') {
      if (read + 2 >= length || !isxdigit((unsigned char)path[read+1]) || !isxdigit((unsigned char)path[read+2]))
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid URL escape");
      char hex[3] = {path[read+1], path[read+2], 0};
      char value = (char)strtoul(hex, nullptr, 16);
      if (!value) return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid URL byte");
      path[write++] = value; read += 2;
    } else path[write++] = path[read];
  }
  path[write] = 0;
  if (!strcmp(path, "/")) strcpy(path, "/index.html");
  const SafeMSWebAsset* asset = nullptr;
  for (const auto& candidate : safeMSWebAssets) {
    if (!strcmp(path, candidate.url)) { asset = &candidate; break; }
  }
  if (!asset) {
    const char* probes[] = {"/generate_204", "/gen_204", "/hotspot-detect.html", "/library/test/success.html", "/connecttest.txt", "/ncsi.txt", "/fwlink", "/canonical.html", "/success.txt", "/redirect"};
    bool captiveProbe = false;
    for (const char* probe : probes) if (!strcmp(path, probe)) captiveProbe = true;
    if (!captiveProbe) return httpd_resp_send_err(request, HTTPD_404_NOT_FOUND, "Not found");
    httpd_resp_set_status(request, "302 Found");
    httpd_resp_set_hdr(request, "Location", "http://192.168.4.1/");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_send(request, "notfall.ms: http://192.168.4.1/", HTTPD_RESP_USE_STRLEN);
  }
  httpd_resp_set_type(request, asset->mime);
  httpd_resp_set_hdr(request, "Cache-Control", "no-cache");
  httpd_resp_set_hdr(request, "X-Content-Type-Options", "nosniff");
  httpd_resp_set_hdr(request, "Vary", "Accept-Encoding");
  // A weak ETag identifies equivalent decoded content across gzip/identity encodings.
  char etag[32]; snprintf(etag, sizeof(etag), "W/%s", asset->etag);
  httpd_resp_set_hdr(request, "ETag", etag);
  char header[128];
  if (httpd_req_get_hdr_value_str(request, "If-None-Match", header, sizeof(header)) == ESP_OK && !strcmp(header, etag)) {
    httpd_resp_set_status(request, "304 Not Modified");
    return httpd_resp_send(request, nullptr, 0);
  }
  const uint8_t* data = asset->data;
  size_t size = asset->size;
  const bool hasRange = httpd_req_get_hdr_value_len(request, "Range") > 0;
  if (!hasRange && asset->gzip && httpd_req_get_hdr_value_str(request, "Accept-Encoding", header, sizeof(header)) == ESP_OK && strstr(header, "gzip") && !strstr(header, "gzip;q=0")) {
    data = asset->gzip; size = asset->gzipSize;
    httpd_resp_set_hdr(request, "Content-Encoding", "gzip");
  }
  // PDF readers may request byte ranges. Serve a valid single range from flash.
  char contentRange[64];
  httpd_resp_set_hdr(request, "Accept-Ranges", "bytes");
  if (hasRange && httpd_req_get_hdr_value_len(request, "If-Range") == 0 &&
      httpd_req_get_hdr_value_str(request, "Range", header, sizeof(header)) == ESP_OK) {
    unsigned long first = 0, last = size ? size - 1 : 0;
    char* end = nullptr;
    bool valid = size > 0 && !strncmp(header, "bytes=", 6) && !strchr(header, ',');
    const char* range = header + 6;
    if (valid && range[0] == '-') {
      unsigned long suffix = strtoul(range + 1, &end, 10);
      valid = end != range + 1 && *end == 0 && suffix > 0;
      first = suffix < size ? size - suffix : 0;
    } else if (valid) {
      first = strtoul(range, &end, 10);
      valid = end != range && *end == '-';
      if (valid && end[1]) {
        const char* tail = end + 1;
        last = strtoul(tail, &end, 10);
        valid = end != tail && *end == 0;
      }
      if (last >= size) last = size - 1;
      valid = valid && first < size && first <= last;
    }
    if (!valid) {
      snprintf(contentRange, sizeof(contentRange), "bytes */%u", (unsigned)size);
      httpd_resp_set_status(request, "416 Range Not Satisfiable");
      httpd_resp_set_hdr(request, "Content-Range", contentRange);
      return httpd_resp_send(request, nullptr, 0);
    }
    snprintf(contentRange, sizeof(contentRange), "bytes %lu-%lu/%u", first, last, (unsigned)size);
    httpd_resp_set_status(request, "206 Partial Content");
    httpd_resp_set_hdr(request, "Content-Range", contentRange);
    data += first; size = last - first + 1;
  }
  return httpd_resp_send(request, (const char*)data, size);
}
}

bool SafeMSKiosk::begin() {
  if (ready) return true;
  if (!incoming) incoming = xQueueCreate(SafeMSFeed::Capacity, sizeof(SafeMSFeed::Alert));
  if (!historyMutex) historyMutex = xSemaphoreCreateMutex();
  if (!incoming || !historyMutex) return false;
  if (!WiFi.mode(WIFI_AP) || !WiFi.softAPConfig(address, address, IPAddress(255,255,255,0)) ||
      !WiFi.softAP("notfall.ms INFO", nullptr, 1, false, 4)) return false;
  WiFi.setSleep(false);
  dns.setErrorReplyCode(DNSReplyCode::NoError);
  if (!dns.start(53, "*", address)) {
    WiFi.softAPdisconnect(true);
    return false;
  }
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.max_open_sockets = 7; // Leave HTTP capacity alongside four live WebSockets.
  config.stack_size = 6144;
  config.lru_purge_enable = true;
  config.recv_wait_timeout = 2;
  config.send_wait_timeout = 2;
  config.uri_match_fn = httpd_uri_match_wildcard;
  if (httpd_start(&server, &config) != ESP_OK) {
    dns.stop(); WiFi.softAPdisconnect(true); return false;
  }
  httpd_uri_t status = {};
  status.uri = "/api/status"; status.method = HTTP_GET; status.handler = statusHandler;
  httpd_uri_t page = {};
  page.uri = "/*"; page.method = HTTP_GET; page.handler = pageHandler;
  httpd_uri_t messages = {};
  messages.uri = "/api/messages"; messages.method = HTTP_GET; messages.handler = messagesHandler;
  httpd_uri_t websocket = {};
  websocket.uri = "/ws"; websocket.method = HTTP_GET; websocket.handler = websocketHandler; websocket.is_websocket = true;
  if (httpd_register_uri_handler(server, &status) != ESP_OK ||
      httpd_register_uri_handler(server, &messages) != ESP_OK ||
      httpd_register_uri_handler(server, &websocket) != ESP_OK ||
      httpd_register_uri_handler(server, &page) != ESP_OK) {
    httpd_stop(server); server = nullptr; dns.stop(); WiFi.softAPdisconnect(true); return false;
  }
  ready = true;
  return true;
}

void SafeMSKiosk::loop() {
  lastMeshTick = millis();
  if (ready) dns.processNextRequest();
  if (ready && uxQueueMessagesWaiting(incoming) && !workPending.exchange(true)) {
    if (httpd_queue_work(server, deliverMessages, nullptr) != ESP_OK) workPending.store(false);
  }
}

bool SafeMSKiosk::setClock(uint32_t seconds) {
  if (seconds < 1767225600UL || seconds >= 4102444800UL) return false; // 2026..2099
  portENTER_CRITICAL(&clockMutex);
  clockOffsetMs = int64_t(seconds)*1000 - esp_timer_get_time()/1000;
  clockSynced = true;
  portEXIT_CRITICAL(&clockMutex);
  return true;
}

void SafeMSKiosk::receiveChannelMessage(const char* channel, const char* text, uint32_t senderTimestamp) {
  if (!channel || strcmp(channel,"Krisenstab")) return;
  SafeMSFeed::Alert alert;
  if (!SafeMSFeed::parse(channel,text,alert)) { ++rejected; return; }
  portENTER_CRITICAL(&clockMutex);
  const bool synced = clockSynced;
  const int64_t now = esp_timer_get_time()/1000 + clockOffsetMs;
  portEXIT_CRITICAL(&clockMutex);
  // No internet is required. After a cold start, use the channel sender's UTC
  // until the operator synchronizes this kiosk via USB or MeshCore device time.
  if (!synced && (senderTimestamp < 1767225600UL || senderTimestamp >= 4102444800UL)) { ++rejected; return; }
  alert.timestampMs = synced ? now : uint64_t(senderTimestamp)*1000;
  uint8_t digest[32];
  mbedtls_sha256_context hash;
  mbedtls_sha256_init(&hash); mbedtls_sha256_starts_ret(&hash,0);
  mbedtls_sha256_update_ret(&hash,reinterpret_cast<const uint8_t*>(&senderTimestamp),4);
  mbedtls_sha256_update_ret(&hash,reinterpret_cast<const uint8_t*>(text),strlen(text));
  mbedtls_sha256_finish_ret(&hash,digest); mbedtls_sha256_free(&hash);
  strcpy(alert.id,"msg-");
  for (int i=0;i<16;++i) snprintf(alert.id+4+i*2,3,"%02x",digest[i]);
  if (!incoming || xQueueSend(incoming,&alert,0)!=pdTRUE) ++dropped;
}

uint32_t SafeMSKiosk::feedSelfTest() { return safeMSFeedSelfTest(); }
uint16_t SafeMSKiosk::readFeed(uint16_t offset, char* output, uint16_t capacity) {
  const auto body = historyMutex ? snapshot() : std::string("{\"messages\":[]}");
  if (offset < body.size()) memcpy(output, body.data()+offset, std::min(size_t(capacity),body.size()-offset));
  return body.size();
}
#endif
