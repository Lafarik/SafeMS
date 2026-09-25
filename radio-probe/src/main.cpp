#include <Arduino.h>
#include <SPI.h>

// Read-only SPI diagnostic for the published Eye v1.3 schematic.
// No RadioLib initialization, register writes, Wi-Fi or transmissions.
static constexpr int CS = 43;
static SPIClass radioSPI(FSPI);

uint8_t readRegister(uint8_t address) {
  radioSPI.beginTransaction(SPISettings(100000, MSBFIRST, SPI_MODE0));
  digitalWrite(CS, LOW);
  radioSPI.transfer(address & 0x7f); // bit 7 clear: read
  uint8_t value = radioSPI.transfer(0);
  digitalWrite(CS, HIGH);
  radioSPI.endTransaction();
  return value;
}

void setup() {
  Serial.begin(115200);
  // Keep the shared-bus microSD deselected; avoid its power switch.
  digitalWrite(41, HIGH);
  pinMode(41, OUTPUT);
  digitalWrite(CS, HIGH);
  pinMode(CS, OUTPUT);
  pinMode(44, INPUT);
  radioSPI.begin(39, 40, 38, CS);
  delay(1000);
}

void loop() {
  uint8_t version = readRegister(0x42);
  uint8_t opmode = readRegister(0x01);
  Serial.printf("SafeMS read-only probe: RegVersion=0x%02X RegOpMode=0x%02X IRQ=%d\n",
                version, opmode, digitalRead(44));
  if (version == 0x12) {
    Serial.println("SX1276/77/78/79-family register signature; exact RF variant not determined.");
  } else if (version == 0x00 || version == 0xff) {
    Serial.println("No valid SX127x signature. Check populated module, wiring and power.");
  } else {
    Serial.println("Unexpected signature: do not flash the MeshCore port yet.");
  }
  delay(2000);
}
