#pragma once
#include <stdint.h>
#include <string.h>
#include <string>
#include <time.h>
#include <stdio.h>

// Hardware-independent, bounded parser/serializer shared by the receiver and tests.
namespace SafeMSFeed {
constexpr size_t MaxText = 160;
constexpr size_t Capacity = 8;
struct Alert {
  char id[37] = {};
  char title[97] = {};
  char message[161] = {};
  uint64_t timestampMs = 0;
};

inline bool validUtf8(const unsigned char* p) {
  while (*p) {
    uint32_t cp = *p++;
    if (cp < 128) {
      if (cp < 32 && cp != '\n' && cp != '\r' && cp != '\t') return false;
      continue;
    }
    unsigned n; uint32_t minimum;
    if (cp >= 0xC2 && cp <= 0xDF) { n=1; minimum=0x80; cp &= 31; }
    else if (cp >= 0xE0 && cp <= 0xEF) { n=2; minimum=0x800; cp &= 15; }
    else if (cp >= 0xF0 && cp <= 0xF4) { n=3; minimum=0x10000; cp &= 7; }
    else return false;
    while (n--) { if ((*p & 0xC0) != 0x80) return false; cp=(cp<<6)|(*p++ & 63); }
    if (cp < minimum || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) return false;
  }
  return true;
}

inline std::string trim(const std::string& s) {
  const auto first = s.find_first_not_of(" \t\r\n");
  return first == std::string::npos ? "" : s.substr(first, s.find_last_not_of(" \t\r\n") - first + 1);
}

inline bool parse(const char* channel, const char* raw, Alert& alert) {
  if (!channel || strcmp(channel, "Krisenstab") || !raw || strnlen(raw, MaxText+1) > MaxText ||
      !validUtf8(reinterpret_cast<const unsigned char*>(raw))) return false;
  std::string text(raw);
  // MeshCore puts "<node name>: " before the user's channel message.
  if (text.compare(0, 6, "title:")) {
    const auto separator = text.find(": ");
    if (separator == std::string::npos || !separator || separator > 31 ||
        text.substr(0, separator).find_first_of("\r\n") != std::string::npos) return false;
    text.erase(0, separator + 2);
  }
  if (text.compare(0, 6, "title:")) return false;
  const auto newline = text.find('\n');
  if (newline == std::string::npos) return false;
  std::string title = trim(text.substr(6, newline-6));
  std::string body = trim(text.substr(newline+1));
  if (body.compare(0, 8, "message:")) return false;
  body = trim(body.substr(8));
  if (title.empty() || title.size() >= sizeof(alert.title) || body.empty() || body.size() >= sizeof(alert.message)) return false;
  // Reject ambiguous duplicate fields; ordinary multiline message text is allowed.
  for (size_t p = 0; p < body.size();) {
    auto end = body.find('\n', p);
    auto line = trim(body.substr(p, end == std::string::npos ? end : end-p));
    if (line.compare(0, 6, "title:") == 0 || line.compare(0, 8, "message:") == 0) return false;
    if (end == std::string::npos) break;
    p = end+1;
  }
  alert = Alert{};
  memcpy(alert.title, title.c_str(), title.size()+1);
  memcpy(alert.message, body.c_str(), body.size()+1);
  return true;
}

inline std::string quote(const char* value) {
  std::string result = "\"";
  const char hex[] = "0123456789abcdef";
  for (const unsigned char* p = reinterpret_cast<const unsigned char*>(value); *p; ++p) {
    if (*p == '"' || *p == '\\') { result += '\\'; result += char(*p); }
    else if (*p < 32) { result += "\\u00"; result += hex[*p>>4]; result += hex[*p&15]; }
    else result += char(*p);
  }
  return result + '"';
}

inline std::string json(const Alert* alerts, size_t count) {
  std::string result = "{\"messages\":[";
  for (size_t i=0; i<count; ++i) {
    const auto& a = alerts[i];
    time_t seconds = a.timestampMs/1000;
    struct tm utc;
    gmtime_r(&seconds, &utc);
    char date[32], timestamp[40];
    strftime(date, sizeof(date), "%Y-%m-%dT%H:%M:%S", &utc);
    snprintf(timestamp, sizeof(timestamp), "%s.%03uZ", date, unsigned(a.timestampMs%1000));
    if (i) result += ',';
    result += "{\"id\":" + quote(a.id) + ",\"title\":" + quote(a.title) +
      ",\"message\":" + quote(a.message) + ",\"timestamp\":" + quote(timestamp) + '}';
  }
  return result + "]}";
}

struct History {
  Alert alerts[Capacity];
  size_t count = 0;
  bool add(const Alert& alert) {
    for (size_t i=0; i<count; ++i) if (!strcmp(alerts[i].id, alert.id)) return false;
    if (count < Capacity) ++count;
    for (size_t i=count-1; i>0; --i) alerts[i] = alerts[i-1];
    alerts[0] = alert;
    return true;
  }
};
}
