#include <Arduino.h>
#include <Wire.h>

// Mặc định phổ biến trên ESP32 thường. Sửa theo bo của bạn.
constexpr int SDA_PIN = 21;
constexpr int SCL_PIN = 22;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);
  Serial.println("\n=== I2C SCANNER ===");
  Serial.printf("SDA=GPIO %d, SCL=GPIO %d\n", SDA_PIN, SCL_PIN);
}

void loop() {
  int found = 0;

  for (uint8_t address = 1; address < 127; ++address) {
    Wire.beginTransmission(address);
    uint8_t error = Wire.endTransmission();
    if (error == 0) {
      Serial.printf("Thiet bi tai 0x%02X\n", address);
      ++found;
    }
  }

  Serial.printf("Tong cong: %d thiet bi\n\n", found);
  delay(3000);
}

