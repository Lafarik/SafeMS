#pragma once
#include <helpers/radiolib/CustomSX1276Wrapper.h>

class EyeRadioWrapper : public CustomSX1276Wrapper {
protected:
  int16_t performChannelScan() override {
    auto& radio = *static_cast<CustomSX1276*>(_radio);
    int16_t result = radio.startChannelScan();
    if (result != RADIOLIB_ERR_NONE) return result;
    // DIO0=CadDone and DIO1=CadDetected share a GPIO. The upstream
    // scanChannel() distinguishes them by pin level, which is unsafe here.
    // Inspect the separate status bits instead. Fail closed on timeout.
    const uint32_t started = millis();
    while (millis() - started < 2000) {
      const uint16_t flags = radio.getIRQFlags();
      if (flags & RADIOLIB_SX127X_CLEAR_IRQ_FLAG_CAD_DETECTED)
        return RADIOLIB_PREAMBLE_DETECTED;
      if (flags & RADIOLIB_SX127X_CLEAR_IRQ_FLAG_CAD_DONE)
        return RADIOLIB_CHANNEL_FREE;
      delay(1);
    }
    radio.standby();
    return RADIOLIB_PREAMBLE_DETECTED;
  }
public:
  EyeRadioWrapper(CustomSX1276& radio, mesh::MainBoard& board)
    : CustomSX1276Wrapper(radio, board) {}
};
