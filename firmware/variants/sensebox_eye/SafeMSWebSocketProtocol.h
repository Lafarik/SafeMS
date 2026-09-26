#pragma once
#include <stddef.h>
#include <string.h>
#include <cJSON.h>

// Read-only browser commands. Parsing this protocol never changes the feed.
namespace SafeMSWebSocketProtocol {
constexpr size_t MaxPayload = 512;
enum class Action { Invalid, Snapshot, Acknowledgement };

inline bool jsonSpace(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

inline bool flatObject(const char* data, size_t length) {
  bool quoted = false, opened = false, closed = false;
  for (size_t i = 0; i < length; ++i) {
    const char c = data[i];
    if (quoted) {
      if (c == '\\') {
        // cJSON strings are C strings, so a decoded NUL must not hide a suffix.
        if (i + 5 < length && !memcmp(data + i, "\\u0000", 6)) return false;
        if (++i >= length) return false;
      } else if (c == '"') quoted = false;
    } else if (c == '"') {
      if (!opened || closed) return false;
      quoted = true;
    } else if (c == '{') {
      if (opened) return false; // Also bounds cJSON recursion to a single object.
      opened = true;
    } else if (c == '}') {
      if (!opened || closed) return false;
      closed = true;
    } else if (c == '[' || c == ']') return false;
    else if ((!opened || closed) && !jsonSpace(c)) return false;
  }
  return opened && closed && !quoted;
}

inline bool boundedString(const cJSON* item, size_t maximum) {
  if (!cJSON_IsString(item) || !item->valuestring) return false;
  const size_t length = strnlen(item->valuestring, maximum + 1);
  if (!length || length > maximum) return false;
  for (size_t i = 0; i < length; ++i)
    if (static_cast<unsigned char>(item->valuestring[i]) < 32) return false;
  return true;
}

inline bool ackTimestamp(const cJSON* item) {
  // The PWA uses Date.toISOString(), including three millisecond digits and Z.
  if (!boundedString(item, 24) || strlen(item->valuestring) != 24) return false;
  const char* value = item->valuestring;
  const char pattern[] = "0000-00-00T00:00:00.000Z";
  for (size_t i = 0; i < 24; ++i)
    if (pattern[i] == '0' ? value[i] < '0' || value[i] > '9' : value[i] != pattern[i]) return false;
  const unsigned year = (value[0]-'0')*1000 + (value[1]-'0')*100 + (value[2]-'0')*10 + value[3]-'0';
  const unsigned month = (value[5]-'0')*10 + value[6]-'0';
  const unsigned day = (value[8]-'0')*10 + value[9]-'0';
  const unsigned hour = (value[11]-'0')*10 + value[12]-'0';
  const unsigned minute = (value[14]-'0')*10 + value[15]-'0';
  const unsigned second = (value[17]-'0')*10 + value[18]-'0';
  if (!month || month > 12 || hour > 23 || minute > 59 || second > 59) return false;
  const unsigned days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  const bool leap = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
  return day && day <= days[month-1] + (month == 2 && leap ? 1 : 0);
}

inline Action parse(const char* data, size_t length) {
  if (!data || !length || length > MaxPayload || memchr(data, 0, length)) return Action::Invalid;
  if (length == 7 && !memcmp(data, "refresh", 7)) return Action::Snapshot;
  if (!flatObject(data, length)) return Action::Invalid;
  const char* end = nullptr;
  cJSON* root = cJSON_ParseWithLengthOpts(data, length, &end, false);
  if (!root) return Action::Invalid;
  // ParseWithLengthOpts can parse a prefix. Permit only JSON whitespace after it.
  bool complete = end && end >= data && end <= data + length;
  if (complete) {
    while (end < data + length && jsonSpace(*end)) ++end;
    complete = end == data + length;
  }
  Action action = Action::Invalid;
  if (complete && cJSON_IsObject(root)) {
    const cJSON* type = cJSON_GetObjectItemCaseSensitive(root, "type");
    if (cJSON_GetArraySize(root) == 1 && boundedString(type, 12) &&
        !strcmp(type->valuestring, "get_messages")) {
      action = Action::Snapshot;
    } else if (cJSON_GetArraySize(root) == 4) {
      const cJSON* messageId = cJSON_GetObjectItemCaseSensitive(root, "messageId");
      const cJSON* deviceId = cJSON_GetObjectItemCaseSensitive(root, "deviceId");
      const cJSON* status = cJSON_GetObjectItemCaseSensitive(root, "status");
      const cJSON* timestamp = cJSON_GetObjectItemCaseSensitive(root, "timestamp");
      // Exact field count plus all four required keys rejects duplicate/extra keys.
      if (boundedString(messageId, 128) && boundedString(deviceId, 128) &&
          boundedString(status, 8) && !strcmp(status->valuestring, "received") && ackTimestamp(timestamp))
        action = Action::Acknowledgement;
    }
  }
  cJSON_Delete(root);
  return action;
}
}
