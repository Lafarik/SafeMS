#pragma once
#include "SafeMSFeed.h"

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
  return failed; // 19 independent checks, zero means all passed.
}
