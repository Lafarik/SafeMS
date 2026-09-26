#ifdef ENABLE_SAFEMS_KIOSK
#include "SafeMSKiosk.h"
#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <esp_http_server.h>
#include <SafeMSWebAssets.h>

namespace {
DNSServer dns;
httpd_handle_t server = nullptr;
bool ready = false;
volatile uint32_t lastMeshTick = 0;
const IPAddress address(192, 168, 4, 1);

esp_err_t statusHandler(httpd_req_t* request) {
  char body[220];
  const uint32_t now = millis();
  const bool meshLoopRecent = lastMeshTick != 0 && uint32_t(now - lastMeshTick) < 1500;
  snprintf(body, sizeof(body),
    "{\"mode\":\"test\",\"uptime_s\":%lu,\"wifi_clients\":%u,\"free_heap\":%u,\"min_free_heap\":%u,\"mesh_loop_recent\":%s}",
    (unsigned long)(now / 1000), WiFi.softAPgetStationNum(), ESP.getFreeHeap(),
    ESP.getMinFreeHeap(), meshLoopRecent ? "true" : "false");
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
  if (!WiFi.mode(WIFI_AP) || !WiFi.softAPConfig(address, address, IPAddress(255,255,255,0)) ||
      !WiFi.softAP("notfall.ms INFO", nullptr, 1, false, 4)) return false;
  WiFi.setSleep(false);
  dns.setErrorReplyCode(DNSReplyCode::NoError);
  if (!dns.start(53, "*", address)) {
    WiFi.softAPdisconnect(true);
    return false;
  }
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.max_open_sockets = 4;
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
  if (httpd_register_uri_handler(server, &status) != ESP_OK ||
      httpd_register_uri_handler(server, &page) != ESP_OK) {
    httpd_stop(server); server = nullptr; dns.stop(); WiFi.softAPdisconnect(true); return false;
  }
  ready = true;
  return true;
}

void SafeMSKiosk::loop() {
  lastMeshTick = millis();
  if (ready) dns.processNextRequest();
}
#endif
