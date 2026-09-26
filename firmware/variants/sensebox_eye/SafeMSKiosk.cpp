#ifdef ENABLE_SAFEMS_KIOSK
#include "SafeMSKiosk.h"
#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <esp_http_server.h>

namespace {
DNSServer dns;
httpd_handle_t server = nullptr;
bool ready = false;
volatile uint32_t lastMeshTick = 0;
const IPAddress address(192, 168, 4, 1);

const char PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="de"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>SafeMS Testkiosk</title><style>
body{font:18px/1.5 system-ui,sans-serif;background:#f4f6f3;color:#172d24;margin:0;padding:24px}
main{max-width:650px;margin:4vh auto;background:white;padding:28px;border-radius:18px}
h1{font-size:2rem;margin:.3em 0}strong{color:#745100}a{color:#075438}
.tag{font-size:14px;font-weight:700;color:#745100}small{color:#52615a}
</style><main><div class="tag">HACKATHON · TESTBETRIEB</div><h1>SafeMS</h1>
<p>Du bist mit dem lokalen Informationskiosk verbunden. Diese Seite funktioniert ohne Internet.</p>
<p><strong>Dies ist eine Testseite. Hier werden noch keine amtlichen Notfallmeldungen bereitgestellt.</strong></p>
<p>Die Anbindung der Hinweise des Krisenstabs wird noch entwickelt.</p>
<p>WLAN: <b>SafeMS-Test</b><br>Lokale Adresse: <a href="http://192.168.4.1/">192.168.4.1</a></p>
<small id="state">Lokale Verbindung aktiv.</small></main>
<script>async function update(){try{const r=await fetch('/api/status',{cache:'no-store'});const s=await r.json();
document.getElementById('state').textContent='Kiosk seit '+Math.floor(s.uptime_s/60)+' Minuten aktiv · '+s.wifi_clients+' WLAN-Gerät(e)';}catch(e){}}
update();setInterval(update,5000);</script></html>)HTML";

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
  if (strcmp(request->uri, "/") != 0) {
    httpd_resp_set_status(request, "302 Found");
    httpd_resp_set_hdr(request, "Location", "http://192.168.4.1/");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_send(request, "SafeMS: http://192.168.4.1/", HTTPD_RESP_USE_STRLEN);
  }
  httpd_resp_set_type(request, "text/html; charset=utf-8");
  httpd_resp_set_hdr(request, "Cache-Control", "no-store");
  return httpd_resp_send(request, PAGE, HTTPD_RESP_USE_STRLEN);
}
}

bool SafeMSKiosk::begin() {
  if (ready) return true;
  if (!WiFi.mode(WIFI_AP) || !WiFi.softAPConfig(address, address, IPAddress(255,255,255,0)) ||
      !WiFi.softAP("SafeMS-Test", nullptr, 1, false, 4)) return false;
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
