#pragma once
#include <helpers/ESP32Board.h>

class SenseBoxEyeBoard : public ESP32Board {
public:
  void begin() {
    // microSD shares SPI. Deselect it before initializing the radio.
    digitalWrite(41, HIGH);
    pinMode(41, OUTPUT);
    digitalWrite(P_LORA_NSS, HIGH);
    pinMode(P_LORA_NSS, OUTPUT);
    ESP32Board::begin();
  }
  uint32_t getIRQGpio() override { return P_LORA_DIO_0; }
  const char* getManufacturerName() const override { return "senseBox Eye (experimental)"; }
};
