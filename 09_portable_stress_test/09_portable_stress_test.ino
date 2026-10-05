#include <Arduino.h>
#include <math.h>

#if defined(ARDUINO_ARCH_ESP32)
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif

// Default: stress for 10 minutes, then report PASS/FAIL.
// Set to 0 for an unlimited run that stops only when the board is reset.
constexpr uint32_t STRESS_DURATION_MINUTES = 10;
constexpr uint32_t REPORT_INTERVAL_MS = 5000UL;
constexpr uint16_t CPU_BATCH_ITERATIONS = 4096;

#if defined(ARDUINO_ARCH_AVR)
constexpr size_t STRESS_BUFFER_SIZE = 256;
#else
constexpr size_t STRESS_BUFFER_SIZE = 4 * 1024;
#endif

static uint8_t stressBuffer[STRESS_BUFFER_SIZE];

volatile bool stressRunning = true;
volatile uint32_t backgroundBatches = 0;
volatile uint32_t backgroundChecksum = 0;
volatile float mainFloatSink = 0.0f;
volatile float backgroundFloatSink = 0.0f;

uint32_t mainBatches = 0;
uint32_t memoryPasses = 0;
uint32_t memoryErrors = 0;
uint32_t startedAtMs = 0;
uint32_t lastReportMs = 0;

void cpuStressBatch(uint32_t &state, float &floatingValue)
{
  for (uint16_t i = 0; i < CPU_BATCH_ITERATIONS; ++i) {
    state = state * 1664525UL + 1013904223UL;
    state ^= state >> 11;
    state += state << 5;

    floatingValue = floatingValue * 1.000031f + 0.000007f;
    if (floatingValue > 20.0f) floatingValue -= 19.0f;
  }
}

bool testMemoryPattern(uint32_t pass)
{
  uint8_t salt = (uint8_t)(pass * 37U + 11U);

  for (size_t i = 0; i < STRESS_BUFFER_SIZE; ++i) {
    stressBuffer[i] = (uint8_t)((i * 31U) ^ salt);
  }

  for (size_t i = 0; i < STRESS_BUFFER_SIZE; ++i) {
    uint8_t expected = (uint8_t)((i * 31U) ^ salt);
    if (stressBuffer[i] != expected) return false;
  }
  return true;
}

#if defined(ARDUINO_ARCH_ESP32)
void backgroundStressTask(void *parameter)
{
  (void)parameter;
  uint32_t state = 0xA5A55A5AUL ^ micros();
  float floatingValue = 0.7654321f;
  uint8_t batchesBeforeYield = 0;

  while (stressRunning) {
    cpuStressBatch(state, floatingValue);
    backgroundChecksum = state;
    backgroundFloatSink = floatingValue;
    ++backgroundBatches;

    // Let the idle task run often enough to service the watchdog.
    if (++batchesBeforeYield >= 8) {
      batchesBeforeYield = 0;
      vTaskDelay(1);
    }
  }

  vTaskDelete(NULL);
}

void startSecondCoreStressIfAvailable()
{
  if (ESP.getChipCores() < 2) {
    Serial.println(F("Second core   : not available"));
    return;
  }

  BaseType_t currentCore = xPortGetCoreID();
  BaseType_t workerCore = currentCore == 0 ? 1 : 0;
  BaseType_t created = xTaskCreatePinnedToCore(
      backgroundStressTask,
      "stress-worker",
      4096,
      NULL,
      1,
      NULL,
      workerCore);

  Serial.print(F("Second core   : "));
  if (created == pdPASS) {
    Serial.print(F("enabled on core "));
    Serial.println(workerCore);
  } else {
    Serial.println(F("task creation failed"));
    ++memoryErrors;
  }
}
#endif

void printStatus(bool finalReport)
{
  uint32_t elapsedSeconds = (millis() - startedAtMs) / 1000UL;

  Serial.print(finalReport ? F("FINAL ") : F("STATUS "));
  Serial.print(F("t="));
  Serial.print(elapsedSeconds);
  Serial.print(F("s, CPU batches="));
  Serial.print(mainBatches);

#if defined(ARDUINO_ARCH_ESP32)
  Serial.print(F("+"));
  Serial.print(backgroundBatches);
  Serial.print(F(", free heap="));
  Serial.print(ESP.getFreeHeap());
  Serial.print(F(" B"));
#endif

  Serial.print(F(", RAM passes="));
  Serial.print(memoryPasses);
  Serial.print(F(", errors="));
  Serial.println(memoryErrors);
}

bool durationReached()
{
  if (STRESS_DURATION_MINUTES == 0) return false;
  const uint32_t targetMs = STRESS_DURATION_MINUTES * 60UL * 1000UL;
  return (uint32_t)(millis() - startedAtMs) >= targetMs;
}

void setup()
{
  Serial.begin(115200);
  delay(1200);

  Serial.println();
  Serial.println(F("=== PORTABLE ARDUINO / ESP32 STRESS TEST ==="));
#if defined(ARDUINO_ARCH_ESP32)
  Serial.print(F("Chip          : "));
  Serial.println(ESP.getChipModel());
  Serial.print(F("CPU cores     : "));
  Serial.println(ESP.getChipCores());
#elif defined(ARDUINO_ARCH_AVR)
  Serial.println(F("Architecture  : AVR"));
#else
  Serial.println(F("Architecture  : generic Arduino"));
#endif
  Serial.print(F("RAM test area : "));
  Serial.print(STRESS_BUFFER_SIZE);
  Serial.println(F(" bytes"));
  Serial.print(F("Duration      : "));
  if (STRESS_DURATION_MINUTES == 0) {
    Serial.println(F("unlimited"));
  } else {
    Serial.print(STRESS_DURATION_MINUTES);
    Serial.println(F(" minutes"));
  }

  startedAtMs = millis();
  lastReportMs = startedAtMs;

#if defined(ARDUINO_ARCH_ESP32)
  startSecondCoreStressIfAvailable();
#endif

  Serial.println(F("Stress test started. Keep Serial Monitor at 115200 baud."));
}

void loop()
{
  static uint32_t cpuState = 0x12345678UL;
  static float floatingValue = 0.1234567f;
#if defined(ARDUINO_ARCH_ESP32)
  static uint8_t batchesBeforeYield = 0;
#endif

  if (!stressRunning) {
    delay(1000);
    return;
  }

  cpuStressBatch(cpuState, floatingValue);
  mainFloatSink = floatingValue;
  ++mainBatches;

  if (!isfinite(floatingValue)) {
    ++memoryErrors;
    floatingValue = 0.1234567f;
  }

  if (!testMemoryPattern(memoryPasses)) {
    ++memoryErrors;
  }
  ++memoryPasses;

#if defined(ARDUINO_ARCH_ESP32)
  // A short periodic delay keeps the FreeRTOS idle task and watchdog healthy.
  if (++batchesBeforeYield >= 8) {
    batchesBeforeYield = 0;
    delay(1);
  }
#endif

  uint32_t now = millis();
  if ((uint32_t)(now - lastReportMs) >= REPORT_INTERVAL_MS) {
    lastReportMs = now;
    printStatus(false);
  }

  if (durationReached()) {
    stressRunning = false;
    delay(10);  // Give the ESP32 background task time to stop.
    printStatus(true);
    Serial.println(memoryErrors == 0 ? F("RESULT: PASS") : F("RESULT: FAIL"));
    Serial.println(F("Reset the board to run again."));
  }
}
