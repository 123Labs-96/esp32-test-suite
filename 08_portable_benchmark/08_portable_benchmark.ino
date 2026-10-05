#include <Arduino.h>
#include <string.h>

// Portable benchmark for Arduino AVR and ESP32.
// No display, Wi-Fi, Bluetooth, or Serial output is used inside timed regions.

#if defined(ARDUINO_ARCH_AVR)
constexpr size_t MEMORY_BUFFER_SIZE = 128;  // Two buffers use only 256 B SRAM.
#else
constexpr size_t MEMORY_BUFFER_SIZE = 8 * 1024;
#endif

constexpr uint32_t TEST_DURATION_US = 1500000UL;
constexpr uint16_t INTEGER_BATCH = 128;
constexpr uint16_t FLOAT_BATCH = 64;
constexpr uint8_t PI_BATCH = 2;

static uint8_t sourceBuffer[MEMORY_BUFFER_SIZE];
static uint8_t targetBuffer[MEMORY_BUFFER_SIZE];

volatile uint32_t integerSink = 0;
volatile float floatSink = 0.0f;
volatile double doubleSink = 0.0;
volatile uint8_t memorySink = 0;
volatile uint8_t piTermCount = 16;

bool benchmarkTimeRemaining(uint32_t startedAt)
{
  return (uint32_t)(micros() - startedAt) < TEST_DURATION_US;
}

double elapsedSeconds(uint32_t startedAt, uint32_t stoppedAt)
{
  return (uint32_t)(stoppedAt - startedAt) / 1000000.0;
}

void printArchitecture()
{
  Serial.print(F("Architecture  : "));
#if defined(ARDUINO_ARCH_ESP32)
  Serial.println(F("ESP32"));
  Serial.print(F("Chip          : "));
  Serial.println(ESP.getChipModel());
  Serial.print(F("CPU cores     : "));
  Serial.println(ESP.getChipCores());
#elif defined(ARDUINO_ARCH_AVR)
  Serial.println(F("AVR"));
#elif defined(ARDUINO_ARCH_RP2040)
  Serial.println(F("RP2040"));
#else
  Serial.println(F("Other Arduino architecture"));
#endif

#ifdef F_CPU
  Serial.print(F("CPU frequency : "));
  Serial.print((double)F_CPU / 1000000.0, 2);
  Serial.println(F(" MHz"));
#endif
  Serial.print(F("sizeof(int)   : "));
  Serial.print(sizeof(int));
  Serial.println(F(" bytes"));
  Serial.print(F("sizeof(double): "));
  Serial.print(sizeof(double));
  Serial.println(F(" bytes"));
}

void runIntegerBenchmark()
{
  uint32_t value = 0x12345678UL ^ micros();
  uint32_t iterations = 0;
  uint32_t startedAt = micros();

  do {
    for (uint16_t i = 0; i < INTEGER_BATCH; ++i) {
      value = value * 1664525UL + 1013904223UL;
      value ^= value >> 13;
      value += value << 7;
    }
    iterations += INTEGER_BATCH;
  } while (benchmarkTimeRemaining(startedAt));

  uint32_t stoppedAt = micros();
  integerSink = value;
  double rate = iterations / elapsedSeconds(startedAt, stoppedAt);

  Serial.print(F("Integer       : "));
  Serial.print(rate / 1000000.0, 3);
  Serial.print(F(" Miter/s, checksum=0x"));
  Serial.println(integerSink, HEX);
}

void runFloatBenchmark()
{
  float value = 0.1234567f + (micros() & 1) * 0.0000001f;
  uint32_t iterations = 0;
  uint32_t startedAt = micros();

  do {
    for (uint16_t i = 0; i < FLOAT_BATCH; ++i) {
      value = value * 1.000123f + 0.000017f;
      if (value > 10.0f) value -= 9.0f;
    }
    iterations += FLOAT_BATCH;
  } while (benchmarkTimeRemaining(startedAt));

  uint32_t stoppedAt = micros();
  floatSink = value;
  double rate = iterations / elapsedSeconds(startedAt, stoppedAt);

  Serial.print(F("Float         : "));
  Serial.print(rate / 1000000.0, 3);
  Serial.print(F(" Miter/s, checksum="));
  Serial.println(floatSink, 7);
}

// BBP converges rapidly; 16 terms are sufficient for a normal 64-bit double.
// noinline + volatile term count prevents link-time constant folding.
__attribute__((noinline)) double calculatePiBBP(uint8_t terms)
{
  double pi = 0.0;
  double factor = 1.0;

  for (uint8_t i = 0; i < terms; ++i) {
    double k = (double)i;
    pi += factor * (4.0 / (8.0 * k + 1.0)
                    - 2.0 / (8.0 * k + 4.0)
                    - 1.0 / (8.0 * k + 5.0)
                    - 1.0 / (8.0 * k + 6.0));
    factor *= 0.0625;  // 1 / 16, without an expensive pow() call.
  }
  return pi;
}

void runPiBenchmark()
{
  uint32_t rounds = 0;
  double checksum = 0.0;
  uint32_t startedAt = micros();

  do {
    for (uint8_t i = 0; i < PI_BATCH; ++i) {
      checksum += calculatePiBBP(piTermCount);
      ++rounds;
    }
  } while (benchmarkTimeRemaining(startedAt));

  uint32_t stoppedAt = micros();
  doubleSink = checksum;
  double rate = rounds / elapsedSeconds(startedAt, stoppedAt);
  double samplePi = calculatePiBBP(piTermCount);

  Serial.print(F("Pi BBP 16-term: "));
  Serial.print(rate, 2);
  Serial.println(F(" calculations/s"));
  Serial.print(F("Pi result     : "));
  Serial.println(samplePi, sizeof(double) >= 8 ? 12 : 6);
  Serial.print(F("Pi checksum   : "));
  Serial.println(doubleSink, 3);
}

void runMemoryBenchmark()
{
  for (size_t i = 0; i < MEMORY_BUFFER_SIZE; ++i) {
    sourceBuffer[i] = (uint8_t)(i * 31U + 7U);
    targetBuffer[i] = 0;
  }

  uint32_t copies = 0;
  uint8_t checksum = 0;
  uint32_t startedAt = micros();

  do {
    memcpy(targetBuffer, sourceBuffer, MEMORY_BUFFER_SIZE);
    checksum ^= targetBuffer[copies & (MEMORY_BUFFER_SIZE - 1)];
    ++copies;
  } while (benchmarkTimeRemaining(startedAt));

  uint32_t stoppedAt = micros();
  memorySink = checksum;
  double seconds = elapsedSeconds(startedAt, stoppedAt);
  double kibPerSecond = ((double)copies * MEMORY_BUFFER_SIZE) / seconds / 1024.0;

  Serial.print(F("memcpy        : "));
  Serial.print(kibPerSecond, 2);
  Serial.print(F(" KiB/s (buffer "));
  Serial.print(MEMORY_BUFFER_SIZE);
  Serial.print(F(" B), checksum=0x"));
  Serial.println(memorySink, HEX);
}

void setup()
{
  Serial.begin(115200);
  delay(1200);

  Serial.println();
  Serial.println(F("=== PORTABLE ARDUINO / ESP32 BENCHMARK ==="));
  printArchitecture();
  Serial.println(F("Each timed test runs for about 1.5 seconds."));
  Serial.println();

  runIntegerBenchmark();
  runFloatBenchmark();
  runPiBenchmark();
  runMemoryBenchmark();

  Serial.println();
  Serial.println(F("Benchmark complete. Reset the board to run again."));
}

void loop()
{
  delay(1000);
}

