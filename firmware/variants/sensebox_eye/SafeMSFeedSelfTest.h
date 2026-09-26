#pragma once
#include "SafeMSFeed.h"
#include "SafeMSWebSocketProtocol.h"

inline bool safeMSWebSocketProtocolSelfTest() {
  using SafeMSWebSocketProtocol::Action;
  bool passed = true;
  auto check = [&](const std::string& input, Action expected) {
    if (SafeMSWebSocketProtocol::parse(input.data(), input.size()) != expected) passed = false;
  };
  const std::string request = "{\"type\":\"get_messages\"}";
  const std::string ack = "{\"messageId\":\"msg-123\",\"deviceId\":\"pwa-11f03e84-23d1-4731-beb2-b7d28ecdbb0b\",\"status\":\"received\",\"timestamp\":\"2026-09-26T12:10:53.027Z\"}";
  check("refresh", Action::Snapshot);
  check(request, Action::Snapshot);
  check(" \t{ \"type\" : \"get_messages\" }\r\n", Action::Snapshot);
  check(ack, Action::Acknowledgement);
  check("{\"timestamp\":\"2024-02-29T00:00:00.000Z\",\"status\":\"received\",\"deviceId\":\"pwa-test\",\"messageId\":\"msg-1\"}", Action::Acknowledgement);
  check("", Action::Invalid);
  check("refresh ", Action::Invalid);
  check("{\"type\":\"get_messages\",\"extra\":1}", Action::Invalid);
  check("{\"type\":\"get_messages\",\"type\":\"get_messages\"}", Action::Invalid);
  check("{\"type\":1}", Action::Invalid);
  check("{\"type\":\"publish\"}", Action::Invalid);
  check("{\"type\":\"get_messages\"", Action::Invalid);
  check(request + "x", Action::Invalid);
  check(request + request, Action::Invalid);
  check(request + std::string(1, '\0'), Action::Invalid);
  check("{\"type\":\"get_messages\\u0000hidden\"}", Action::Invalid);
  check("{\"type\\u0000hidden\":\"get_messages\"}", Action::Invalid);
  check("{\"type\":{\"type\":\"get_messages\"}}", Action::Invalid);
  check("[" + request + "]", Action::Invalid);
  check(request + std::string(SafeMSWebSocketProtocol::MaxPayload-request.size(), ' '), Action::Snapshot);
  check(request + std::string(SafeMSWebSocketProtocol::MaxPayload-request.size()+1, ' '), Action::Invalid);
  const std::string longAckPrefix = "{\"messageId\":\"";
  const std::string ackSuffix = "\",\"deviceId\":\"pwa-test\",\"status\":\"received\",\"timestamp\":\"2026-09-26T12:10:53.027Z\"}";
  check(longAckPrefix + std::string(128, 'x') + ackSuffix, Action::Acknowledgement);
  check(longAckPrefix + std::string(129, 'x') + ackSuffix, Action::Invalid);
  check(longAckPrefix + ackSuffix, Action::Invalid);
  check("{\"messageId\":\"msg-1\",\"deviceId\":\"" + std::string(129, 'x') +
    "\",\"status\":\"received\",\"timestamp\":\"2026-09-26T12:10:53.027Z\"}", Action::Invalid);
  check("{\"messageId\":1,\"deviceId\":\"pwa-test\",\"status\":\"received\",\"timestamp\":\"2026-09-26T12:10:53.027Z\"}", Action::Invalid);
  check("{\"messageId\":\"msg-1\",\"messageId\":\"msg-2\",\"status\":\"received\",\"timestamp\":\"2026-09-26T12:10:53.027Z\"}", Action::Invalid);
  check("{\"messageId\":\"msg-1\",\"deviceId\":\"pwa-test\",\"status\":\"sent\",\"timestamp\":\"2026-09-26T12:10:53.027Z\"}", Action::Invalid);
  check("{\"messageId\":\"msg-1\",\"deviceId\":\"pwa-test\",\"status\":\"received\",\"timestamp\":\"2026-02-29T12:10:53.027Z\"}", Action::Invalid);
  check("{\"messageId\":\"msg-1\",\"deviceId\":\"pwa-test\",\"status\":\"received\",\"timestamp\":\"2026-09-26T24:10:53.027Z\"}", Action::Invalid);
  check("{\"messageId\":\"msg-1\",\"deviceId\":\"pwa-test\",\"status\":\"received\",\"timestamp\":\"today\"}", Action::Invalid);
  check("{\"messageId\":\"msg-1\\u0000hidden\",\"deviceId\":\"pwa-test\",\"status\":\"received\",\"timestamp\":\"2026-09-26T12:10:53.027Z\"}", Action::Invalid);
  check("{\"messageId\":\"msg-1\",\"deviceId\":\"pwa-\\ntest\",\"status\":\"received\",\"timestamp\":\"2026-09-26T12:10:53.027Z\"}", Action::Invalid);
  return passed;
}

// Read-only USB diagnostics: exercise the same parser and serializer used on air.
inline uint32_t safeMSFeedSelfTest() {
  using namespace SafeMSFeed;
  uint32_t failed = 0; unsigned n=0;
  auto check = [&](bool ok) { if (!ok) failed |= uint32_t(1)<<n; ++n; };
  Alert a;
  check(parse("Krisenstab", "Leitstelle: title: Stromausfall\nmessage: Hier ist dick Stromausfall, Leute!!", a) && !strcmp(a.title,"Stromausfall") && !strcmp(a.message,"Hier ist dick Stromausfall, Leute!!"));
  check(parse("Krisenstab", "title: Wasser\r\nmessage: F\xC3\xBCr alle: \"hier\" \\ dort\nzweite Zeile", a));
  strcpy(a.id,"msg-test"); a.timestampMs=1790417920123ULL;
  const auto encoded=json(&a,1);
  check(encoded.find("\\\"hier\\\"") != std::string::npos && encoded.find("\\\\ dort\\u000azweite") != std::string::npos);
  check(encoded.find("2026-09-26T10:18:40.123Z") != std::string::npos);
  check(!parse("Public", "title: A\nmessage: B",a));
  check(!parse("Krisenstab", "normale Nachricht",a));
  check(!parse("Krisenstab", "title: A",a));
  check(!parse("Krisenstab", "title: \nmessage: B",a));
  check(!parse("Krisenstab", "title: A\nmessage: ",a));
  check(!parse("Krisenstab", "title: A\nmessage: B\ntitle: C",a));
  check(!parse("Krisenstab", "title: A\nmessage: B\nmessage: C",a));
  check(!parse("Krisenstab", "title: A\nmessage: \xC0\xAF",a));
  check(!parse("Krisenstab", "title: A\nmessage: \xED\xA0\x80",a));
  check(!parse("Krisenstab", "title: A\nmessage: \xF0\x9F",a));
  std::string limit="title: A\nmessage: "; limit.append(MaxText-limit.size(),'x');
  check(parse("Krisenstab",limit.c_str(),a));
  limit+='x'; check(!parse("Krisenstab",limit.c_str(),a));
  check(json(nullptr,0)=="{\"messages\":[]}");
  History history; strcpy(a.id,"one"); check(history.add(a) && !history.add(a) && history.count==1);
  for(unsigned i=0;i<10;++i) { snprintf(a.id,sizeof(a.id),"msg-%u",i); history.add(a); }
  check(history.count==8 && !strcmp(history.alerts[0].id,"msg-9") && !strcmp(history.alerts[7].id,"msg-2"));
  // Inline fields work both with and without MeshCore's sender prefix.
  check(parse("Krisenstab", "title: Stromausfall message: Hier ist dick Stromausfall, Leute!!", a) &&
    !strcmp(a.title,"Stromausfall") && !strcmp(a.message,"Hier ist dick Stromausfall, Leute!!"));
  check(parse("Krisenstab", "Leitstelle: title: Stromausfall message: Hier ist dick Stromausfall, Leute!!", a) &&
    !strcmp(a.title,"Stromausfall") && !strcmp(a.message,"Hier ist dick Stromausfall, Leute!!"));
  check(parse("Krisenstab", "title:\tWasser\t\tmessage:\tHier", a) && !strcmp(a.title,"Wasser") && !strcmp(a.message,"Hier") &&
    parse("Krisenstab", "title: Wasser\rmessage: Hier", a) &&
    parse("Krisenstab", "title: Wasser \r\n\tmessage: Hier", a));
  check(parse("Krisenstab", "title: Wasser\\nmessage: Hier\\nbleibt Text", a) &&
    !strcmp(a.title,"Wasser") && !strcmp(a.message,"Hier\\nbleibt Text") &&
    parse("Krisenstab", "Leitstelle: title: Wasser\\n message: Hier", a) && !strcmp(a.title,"Wasser"));
  check(parse("Krisenstab", "title: errormessage: Code message: B errormessage: C", a) &&
    !strcmp(a.title,"errormessage: Code") && !strcmp(a.message,"B errormessage: C"));
  check(!parse("Krisenstab", "title: A errormessage: B",a));
  check(!parse("Krisenstab", "title: message: B",a) && !parse("Krisenstab", "title:\\nmessage: B",a));
  check(!parse("Krisenstab", "title: A message: \t",a));
  check(!parse("Krisenstab", "title: A message: B message: C",a) &&
    !parse("Krisenstab", "title: A message: B\\nmessage: C",a));
  check(!parse("Krisenstab", "title: A title: B message: C",a) &&
    !parse("Krisenstab", "title: A message: B title: C",a));
  check(!parse("Krisenstab", "Title: A message: B",a) && !parse("Krisenstab", "title: A Message: B",a) &&
    !parse("Krisenstab", "title: Amessage: B",a) && !parse("Krisenstab", "title: A\nextra title line\nmessage: B",a));
  check(parse("Krisenstab", "title: Wasser message: Erste Zeile\nZweite Zeile\r\nDritte Zeile", a) &&
    !strcmp(a.message,"Erste Zeile\nZweite Zeile\r\nDritte Zeile"));
  const std::string longestTitle(96, 'T');
  check(parse("Krisenstab", ("title: " + longestTitle + " message: B").c_str(),a) &&
    !parse("Krisenstab", ("title: " + longestTitle + "T message: B").c_str(),a));
  // Keep USB114's existing 32-bit response: bit31 also covers the read-only WS protocol.
  if (!safeMSWebSocketProtocolSelfTest()) failed |= uint32_t(1) << 31;
  return failed; // 32 diagnostic groups; zero means all feed and WebSocket checks passed.
}
