#include <Arduino.h>
#include <cstring>

constexpr int BENCH_GPIO_PIN = 18;  // Sửa nếu chân này không phù hợp với bo.
constexpr uint32_t INTEGER_ITERATIONS = 5000000;
constexpr uint32_t FLOAT_ITERATIONS = 1000000;
constexpr uint32_t GPIO_TOGGLES = 200000;
constexpr size_t BUFFER_SIZE = 32 * 1024;
constexpr uint32_t COPY_REPEATS = 1000;

volatile uint32_t integerResult = 0;
volatile float floatResult = 0.0f;
volatile uint8_t memoryResult = 0;

double elapsedSeconds(uint32_t startUs, uint32_t endUs) {
  return (uint32_t)(endUs - startUs) / 1000000.0;
}

void integerBenchmark() {
  uint32_t value = 0x12345678;
  uint32_t startUs = micros();

  for (uint32_t i = 0; i < INTEGER_ITERATIONS; ++i) {
    value = value * 1664525u + 1013904223u;
    value ^= value >> 13;
  }

  uint32_t endUs = micros();
  integerResult = value;
  double seconds = elapsedSeconds(startUs, endUs);
  Serial.printf("Integer : %.2f Miter/s, checksum=0x%08X\n",
                INTEGER_ITERATIONS / seconds / 1000000.0, integerResult);
}

void floatBenchmark() {
  float x = 0.1234567f;
  uint32_t startUs = micros();

  for (uint32_t i = 0; i < FLOAT_ITERATIONS; ++i) {
    x = x * 1.000001f + 0.000001f;
    if (x > 10.0f) x -= 9.0f;
  }

  uint32_t endUs = micros();
  floatResult = x;
  double seconds = elapsedSeconds(startUs, endUs);
  Serial.printf("Float   : %.2f Miter/s, checksum=%.6f\n",
                FLOAT_ITERATIONS / seconds / 1000000.0, floatResult);
}

void memoryBenchmark() {
  uint8_t *source = (uint8_t *)malloc(BUFFER_SIZE);
  uint8_t *target = (uint8_t *)malloc(BUFFER_SIZE);
  if (!source || !target) {
    Serial.println("Memory  : SKIP (khong cap phat duoc buffer)");
    free(source);
    free(target);
    return;
  }

  for (size_t i = 0; i < BUFFER_SIZE; ++i) source[i] = (uint8_t)i;

  uint32_t startUs = micros();
  for (uint32_t i = 0; i < COPY_REPEATS; ++i) {
    memcpy(target, source, BUFFER_SIZE);
    memoryResult ^= target[i & (BUFFER_SIZE - 1)];
  }
  uint32_t endUs = micros();

  double seconds = elapsedSeconds(startUs, endUs);
  double totalMiB = (double)BUFFER_SIZE * COPY_REPEATS / (1024.0 * 1024.0);
  Serial.printf("memcpy  : %.2f MiB/s, checksum=0x%02X\n",
                totalMiB / seconds, memoryResult);

  free(source);
  free(target);
}

void gpioBenchmark() {
  pinMode(BENCH_GPIO_PIN, OUTPUT);
  uint32_t startUs = micros();

  for (uint32_t i = 0; i < GPIO_TOGGLES; ++i) {
    digitalWrite(BENCH_GPIO_PIN, i & 1);
  }

  uint32_t endUs = micros();
  double seconds = elapsedSeconds(startUs, endUs);
  Serial.printf("GPIO    : %.2f M digitalWrite/s (GPIO %d)\n",
                GPIO_TOGGLES / seconds / 1000000.0, BENCH_GPIO_PIN);
}

bool testMemoryRegion(uint8_t *buffer, size_t size) {
  for (size_t i = 0; i < size; ++i) buffer[i] = (uint8_t)(i * 31u + 7u);
  for (size_t i = 0; i < size; ++i) {
    if (buffer[i] != (uint8_t)(i * 31u + 7u)) return false;
  }
  return true;
}

void psramTest() {
  size_t psramSize = ESP.getPsramSize();
  if (psramSize == 0) {
    Serial.println("PSRAM   : khong co/khong bat");
    return;
  }

  const size_t testSize = 1024 * 1024;
  uint8_t *buffer = (uint8_t *)ps_malloc(testSize);
  if (!buffer) {
    Serial.println("PSRAM   : FAIL (khong cap phat duoc 1 MiB)");
    return;
  }

  bool ok = testMemoryRegion(buffer, testSize);
  Serial.printf("PSRAM   : %s (kiem tra 1 MiB / tong %u bytes)\n",
                ok ? "PASS" : "FAIL", psramSize);
  free(buffer);
}

void runBenchmarks() {
  Serial.println("\n=== ESP32 BENCHMARK ===");
  Serial.printf("Chip: %s rev %d, %d core, CPU %u MHz\n",
                ESP.getChipModel(), ESP.getChipRevision(),
                ESP.getChipCores(), ESP.getCpuFreqMHz());
  Serial.printf("Free heap before: %u bytes\n", ESP.getFreeHeap());

  integerBenchmark();
  floatBenchmark();
  memoryBenchmark();
  gpioBenchmark();
  psramTest();

  Serial.printf("Free heap after : %u bytes\n", ESP.getFreeHeap());
  Serial.println("Benchmark xong. Reset bo de chay lai.");
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  runBenchmarks();
}

void loop() {
  delay(1000);
}

