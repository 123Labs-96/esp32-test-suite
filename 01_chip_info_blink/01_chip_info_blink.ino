#include <Arduino.h>

// Sửa theo bo của bạn. Nhiều bo ESP32 thường dùng GPIO 2.
#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif
constexpr int LED_PIN = LED_BUILTIN;

void printChipInfo() {
  uint64_t mac = ESP.getEfuseMac();

  Serial.println("\n=== ESP32 CHIP INFO ===");
  Serial.printf("Chip          : %s\n", ESP.getChipModel());
  Serial.printf("Revision      : %d\n", ESP.getChipRevision());
  Serial.printf("CPU cores     : %d\n", ESP.getChipCores());
  Serial.printf("CPU frequency : %u MHz\n", ESP.getCpuFreqMHz());
  Serial.printf("Flash size    : %u bytes\n", ESP.getFlashChipSize());
  Serial.printf("Sketch size   : %u bytes\n", ESP.getSketchSize());
  Serial.printf("Free sketch   : %u bytes\n", ESP.getFreeSketchSpace());
  Serial.printf("Heap total    : %u bytes\n", ESP.getHeapSize());
  Serial.printf("Heap free     : %u bytes\n", ESP.getFreeHeap());
  Serial.printf("Heap minimum  : %u bytes\n", ESP.getMinFreeHeap());
  Serial.printf("Largest block : %u bytes\n", ESP.getMaxAllocHeap());
  Serial.printf("PSRAM total   : %u bytes\n", ESP.getPsramSize());
  Serial.printf("PSRAM free    : %u bytes\n", ESP.getFreePsram());
  Serial.printf("eFuse MAC     : %04X%08X\n",
                (uint16_t)(mac >> 32), (uint32_t)mac);
  Serial.printf("LED pin       : GPIO %d\n", LED_PIN);
  Serial.println("=======================");
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  pinMode(LED_PIN, OUTPUT);
  printChipInfo();
}

void loop() {
  digitalWrite(LED_PIN, HIGH);
  Serial.println("LED ON");
  delay(500);
  digitalWrite(LED_PIN, LOW);
  Serial.println("LED OFF");
  delay(500);
}

