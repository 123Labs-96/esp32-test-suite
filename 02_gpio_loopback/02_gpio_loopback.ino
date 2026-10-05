#include <Arduino.h>

// Ví dụ phù hợp với nhiều bo ESP32 thường.
// Sửa chân theo bo, rồi nối OUT -> điện trở 1 kΩ -> IN.
constexpr int GPIO_OUT_PIN = 18;
constexpr int GPIO_IN_PIN  = 19;

bool checkLevel(uint8_t outputLevel) {
  digitalWrite(GPIO_OUT_PIN, outputLevel);
  delayMicroseconds(20);
  int inputLevel = digitalRead(GPIO_IN_PIN);

  Serial.printf("OUT=%s, IN=%s : %s\n",
                outputLevel ? "HIGH" : "LOW",
                inputLevel ? "HIGH" : "LOW",
                inputLevel == outputLevel ? "PASS" : "FAIL");
  return inputLevel == outputLevel;
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(GPIO_OUT_PIN, OUTPUT);
  pinMode(GPIO_IN_PIN, INPUT_PULLDOWN);

  Serial.println("\n=== GPIO LOOPBACK TEST ===");
  Serial.printf("Noi GPIO %d qua dien tro 1 kOhm sang GPIO %d.\n",
                GPIO_OUT_PIN, GPIO_IN_PIN);
}

void loop() {
  bool lowOk  = checkLevel(LOW);
  bool highOk = checkLevel(HIGH);

  Serial.printf("GPIO loopback: %s\n\n", lowOk && highOk ? "PASS" : "FAIL");
  delay(1000);
}

