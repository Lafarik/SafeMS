#pragma once
#include <helpers/ESP32Board.h>
#include <helpers/ui/SSD1306Display.h>

extern SSD1306Display display;

class SenseBoxEyeBoard : public ESP32Board {
public:
  void begin() {
    // microSD shares SPI. Deselect it before initializing the radio.
    digitalWrite(41, HIGH);
    pinMode(41, OUTPUT);
    digitalWrite(P_LORA_NSS, HIGH);
    pinMode(P_LORA_NSS, OUTPUT);
    ESP32Board::begin();

    // Initialize the OLED display
    if (display.begin()) {
      display.clearDisplay();
      display.setCursor(0, 0);
      display.setTextSize(1);
      display.setTextColor(WHITE, BLACK);
      display.println("Hello Mesh");
      display.display();
    }
  }
  uint32_t getIRQGpio() override { return P_LORA_DIO_0; }
  const char* getManufacturerName() const override { return "senseBox Eye (experimental)"; }
};
