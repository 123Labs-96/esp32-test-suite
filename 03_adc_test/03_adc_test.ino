#include <Arduino.h>

// GPIO 34 là ADC1 trên ESP32 thường, nhưng không tồn tại trên mọi biến thể.
constexpr int ADC_PIN = 34;
constexpr int SAMPLE_COUNT = 64;

void setup() {
  Serial.begin(115200);
  delay(1000);
  analogReadResolution(12);  // Thang raw 0..4095

  Serial.println("\n=== ADC TEST ===");
  Serial.printf("ADC pin: GPIO %d\n", ADC_PIN);
  Serial.println("Thu noi GND, sau do dung bien tro 0..3.3 V.");
  Serial.println("Khong dua 5 V vao GPIO!");
}

void loop() {
  uint32_t rawSum = 0;
  uint32_t mvSum = 0;

  for (int i = 0; i < SAMPLE_COUNT; ++i) {
    rawSum += analogRead(ADC_PIN);
    mvSum += analogReadMilliVolts(ADC_PIN);
    delay(2);
  }

  float rawAverage = rawSum / (float)SAMPLE_COUNT;
  float mvAverage = mvSum / (float)SAMPLE_COUNT;
  Serial.printf("raw(avg)=%.1f / 4095, calibrated(avg)=%.1f mV\n",
                rawAverage, mvAverage);
  delay(500);
}

