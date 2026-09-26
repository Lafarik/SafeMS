#pragma once
#include <stdint.h>

namespace SafeMSKiosk {
bool begin();
void loop();
void receiveChannelMessage(const char* channel, const char* text, uint32_t senderTimestamp);
bool setClock(uint32_t epochSeconds);
uint32_t feedSelfTest();
uint16_t readFeed(uint16_t offset, char* output, uint16_t capacity);
}
