#include <Arduino.h>
#include "target.h"

SenseBoxEyeBoard board;
static SPIClass radio_spi(FSPI);
// DIO0 and DIO1 are diode-ORed onto GPIO44 in the published v1.3 schematic.
// Reset is RC-controlled, not connected to an MCU pin.
static CustomSX1276 radio = new Module(P_LORA_NSS, P_LORA_DIO_0,
                                      RADIOLIB_NC, P_LORA_DIO_1, radio_spi);
EyeRadioWrapper radio_driver(radio, board);
ESP32RTCClock fallback_clock;
AutoDiscoverRTCClock rtc_clock(fallback_clock);
EnvironmentSensorManager sensors;
#ifdef DISPLAY_CLASS
DISPLAY_CLASS display;
#endif

bool radio_init() {
  fallback_clock.begin();
  rtc_clock.begin(Wire);
  return radio.std_init(&radio_spi);
}

mesh::LocalIdentity radio_new_identity() {
  RadioNoiseListener rng(radio);
  return mesh::LocalIdentity(&rng);
}
