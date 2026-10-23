// EEMBC CoreMark 1.0: 1/2 cores, SRAM/PSRAM/hybrid, SH1106 OLED.
// OLED runs live only when the worker is on core 1. It is frozen throughout
// dual-core runs so both measured workers get the cores to themselves.

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <esp_heap_caps.h>
#include <esp_task_wdt.h>
#include <esp32-hal-psram.h>
#include <freertos/task.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#if !defined(ARDUINO_ARCH_ESP32)
#error This sketch requires esp32 by Espressif Systems.
#endif

#if defined(CONFIG_IDF_TARGET_ESP32)
constexpr int OLED_SDA = 21;
constexpr int OLED_SCL = 22;
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
constexpr int OLED_SDA = 8;
constexpr int OLED_SCL = 9;
#else
#error This sketch targets dual-core ESP32 or ESP32-S3 boards.
#endif

constexpr uint8_t OLED_ADDRESS = 0x3C;
constexpr uint32_t OLED_REFRESH_MS = 2000;
constexpr uint32_t BENCHMARK_WDT_TIMEOUT_MS = 120000;
constexpr bool ENABLE_OLED = true;  // Set false for a Serial-only comparison run.

Adafruit_SH1106G oled(128, 64, &Wire, -1, 400000, 400000);
bool oledReady = false;
enum RunKind : uint8_t {
  SINGLE_SRAM, SINGLE_PSRAM, DUAL_SRAM, DUAL_PSRAM, DUAL_HYBRID, FINISHED
};
enum MemoryMode : uint8_t { RAM_SRAM, RAM_PSRAM, RAM_HYBRID };
volatile uint8_t currentRun = SINGLE_SRAM;
volatile bool oledTaskStop = false;
TaskHandle_t oledTaskHandle = nullptr;
SemaphoreHandle_t oledTaskExited = nullptr;
bool psramAvailable = false;
uint32_t measuredMhz = 0;
uint8_t completedRuns = 0;
uint8_t totalRuns = 2;
char oledChipLine[22] = {};
char oledMemoryLine[22] = {};
char oledFinalLine[22] = {};
struct BenchmarkResult {
  bool pass;
  double coremarksPerSecond;
  double coremarksPerMhz;
  double seconds;
};
BenchmarkResult results[5] = {};

extern "C" int coremark_main(void);
extern "C" bool coremark_prepare_run(uint8_t contexts, uint8_t memoryMode);
extern "C" void coremark_release_run(void);
extern "C" bool coremark_port_failed(void);
extern "C" bool coremark_measurement_active(void);
extern "C" uint32_t coremark_measurement_start_ms(void);
extern "C" uint32_t coremark_estimated_duration_ms(void);

static bool logValidated = false;
static bool logError = false;
static double logScore = 0.0;
static double logSeconds = 0.0;

void printMBytes(uint32_t bytes) {
  Serial.print(bytes / (1024UL * 1024UL));
  Serial.print(F(" MiB"));
}

void printModuleInfo() {
  Serial.println(F("\n=== COREMARK SINGLE / DUAL + SH1106 ==="));
  Serial.printf("Chip: %s rev %d, cores: %d, CPU: %u MHz\n",
                ESP.getChipModel(), ESP.getChipRevision(),
                ESP.getChipCores(), ESP.getCpuFreqMHz());
  Serial.print(F("Flash: "));
  printMBytes(ESP.getFlashChipSize());
  Serial.print(F(", PSRAM: "));
  printMBytes(ESP.getPsramSize());
  Serial.println();
  Serial.printf("Free internal heap: %u B, free PSRAM: %u B\n",
                heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                ESP.getFreePsram());
  Serial.printf("OLED: SH1106 128x64, SDA=%d, SCL=%d, address=0x%02X\n",
                OLED_SDA, OLED_SCL, OLED_ADDRESS);
  Serial.println(F("CoreMark: 1 or 2 contexts, seeds 0/0/0x66, 2 KB per context"));
  Serial.println(F("CoreMark/MHz = aggregate CoreMark/s / clock MHz of each core"));
}

// Keep the idle tasks subscribed. Removing them with disableCore*WDT() can
// leave the IDF idle hook feeding an unsubscribed task and flood Serial with
// "esp_task_wdt_reset(...): task not found" on Arduino-ESP32 3.x.
esp_task_wdt_config_t normalWatchdogConfig() {
  uint32_t idleMask = 0;
#if defined(CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU0) && CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU0
  idleMask |= 1U << 0;
#endif
#if defined(CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU1) && CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU1
  idleMask |= 1U << 1;
#endif
#if defined(CONFIG_ESP_TASK_WDT_PANIC) && CONFIG_ESP_TASK_WDT_PANIC
  constexpr bool panicOnTimeout = true;
#else
  constexpr bool panicOnTimeout = false;
#endif
  return {CONFIG_ESP_TASK_WDT_TIMEOUT_S * 1000U, idleMask, panicOnTimeout};
}

void drawHeader() {
  oled.setTextColor(SH110X_WHITE);
  oled.setTextSize(1);
  oled.setCursor(0, 0);
  oled.print(oledChipLine);
  oled.setCursor(0, 11);
  oled.print(oledMemoryLine);
}

const __FlashStringHelper *runLabel(RunKind kind) {
  switch (kind) {
    case SINGLE_SRAM: return F("1C SRAM");
    case SINGLE_PSRAM: return F("1C PSRAM");
    case DUAL_SRAM: return F("2C SRAM");
    case DUAL_PSRAM: return F("2C PSRAM");
    default: return F("2C SRAM+PSRAM");
  }
}

void drawBar(uint8_t percent) {
  oled.drawRect(0, 39, 128, 10, SH110X_WHITE);
  if (percent > 0) oled.fillRect(2, 41, percent * 124 / 100, 6, SH110X_WHITE);
}

void drawRunningScreen() {
  if (!oledReady) return;
  oled.clearDisplay();
  drawHeader();
  oled.setCursor(0, 24);
  oled.print(runLabel(static_cast<RunKind>(
      __atomic_load_n(&currentRun, __ATOMIC_ACQUIRE))));

  uint32_t elapsed = 0;
  uint32_t estimated = 0;
  uint8_t percent = 0;
  if (coremark_measurement_active()) {
    elapsed = millis() - coremark_measurement_start_ms();
    estimated = coremark_estimated_duration_ms();
    if (estimated > 0) {
      percent = (uint8_t)min(99UL, (uint32_t)((uint64_t)elapsed * 100 / estimated));
    }
  }

  drawBar(percent);
  oled.setCursor(0, 53);
  oled.print(completedRuns + 1);
  oled.print('/');
  oled.print(totalRuns);
  oled.print(' ');
  if (coremark_measurement_active()) {
    oled.print(percent);
    oled.print(F("%  "));
    oled.print(elapsed / 1000);
    oled.print(F("s est."));
  } else {
    oled.print(F("Calibrating"));
  }
  oled.display();
}

// For dual-core runs this is the last OLED transfer before CoreMark starts.
// The next transfer happens only after coremark_main() returns.
void drawStaticStageScreen(RunKind kind) {
  if (!oledReady) return;
  oled.clearDisplay();
  drawHeader();
  oled.setCursor(0, 24);
  oled.print(runLabel(kind));
  drawBar((uint8_t)(completedRuns * 100 / totalRuns));
  oled.setCursor(0, 53);
  oled.print(completedRuns + 1);
  oled.print('/');
  oled.print(totalRuns);
  oled.print(F("  OLED PAUSED"));
  oled.display();
}

void drawScoreRow(uint8_t y, const __FlashStringHelper *label, RunKind kind) {
  oled.setCursor(0, y);
  oled.print(label);
  if (!psramAvailable && (kind == SINGLE_PSRAM || kind == DUAL_PSRAM ||
                          kind == DUAL_HYBRID)) {
    oled.print(F("N/A"));
  } else if (results[kind].pass) {
    oled.print(results[kind].coremarksPerMhz, 2);
  } else {
    oled.print(F("FAIL"));
  }
}

void drawFinalScreen() {
  if (!oledReady) return;
  oled.clearDisplay();
  oled.setTextColor(SH110X_WHITE);
  oled.setTextSize(1);
  oled.setCursor(0, 0);
  oled.print(oledChipLine);
  oled.setCursor(0, 11);
  oled.print(oledFinalLine);
  drawScoreRow(20, F("1C SRAM  "), SINGLE_SRAM);
  drawScoreRow(29, F("1C PSRAM "), SINGLE_PSRAM);
  drawScoreRow(38, F("2C SRAM  "), DUAL_SRAM);
  drawScoreRow(47, F("2C PSRAM "), DUAL_PSRAM);
  drawScoreRow(56, F("2C MIX   "), DUAL_HYBRID);
  oled.display();
}

void oledTask(void *parameter) {
  (void)parameter;
  while (!__atomic_load_n(&oledTaskStop, __ATOMIC_ACQUIRE)) {
    drawRunningScreen();
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(OLED_REFRESH_MS));
  }
  xSemaphoreGive(oledTaskExited);
  vTaskDelete(NULL);
}

void startLiveOled() {
  if (!oledReady || oledTaskExited == nullptr) return;
  __atomic_store_n(&oledTaskStop, false, __ATOMIC_RELEASE);
  if (xTaskCreatePinnedToCore(oledTask, "coremark-oled", 4096, nullptr, 3,
                              &oledTaskHandle, 0) != pdPASS) {
    oledTaskHandle = nullptr;
    Serial.println(F("OLED task creation failed; live progress disabled."));
  }
}

void stopLiveOled() {
  if (oledTaskHandle == nullptr) return;
  __atomic_store_n(&oledTaskStop, true, __ATOMIC_RELEASE);
  xTaskNotifyGive(oledTaskHandle);
  xSemaphoreTake(oledTaskExited, portMAX_DELAY);
  oledTaskHandle = nullptr;
}

// The CoreMark C sources call this function. Store only the fields needed for
// the OLED summary; the complete original report still goes to Serial.
extern "C" int ee_printf(const char *format, ...) {
  char line[256];
  va_list args;
  va_start(args, format);
  int count = vsnprintf(line, sizeof(line), format, args);
  va_end(args);
  if (count < 0) return count;

  Serial.print(line);
  if (strstr(line, "Correct operation validated") != nullptr) logValidated = true;
  if (strstr(line, "ERROR") != nullptr || strstr(line, "Errors detected") != nullptr) {
    logError = true;
  }
  if (strncmp(line, "Total time (secs): ", 19) == 0) {
    logSeconds = strtod(line + 19, nullptr);
  }
  if (strncmp(line, "CoreMark 1.0 : ", 15) == 0) {
    logScore = strtod(line + 15, nullptr);
  }
  return count;
}

extern "C" uint32_t Arduino_micros(void) {
  return micros();
}

bool runCoremarkPass(RunKind kind) {
  const bool single = kind == SINGLE_SRAM || kind == SINGLE_PSRAM;
  const uint8_t contexts = single ? 1 : 2;
  const uint8_t memoryMode = kind == DUAL_HYBRID ? RAM_HYBRID
      : (kind == SINGLE_PSRAM || kind == DUAL_PSRAM ? RAM_PSRAM : RAM_SRAM);
  __atomic_store_n(&currentRun, static_cast<uint8_t>(kind), __ATOMIC_RELEASE);
  logValidated = false;
  logError = false;
  logScore = 0.0;
  logSeconds = 0.0;

  if (!single) drawStaticStageScreen(kind);

  if (!coremark_prepare_run(contexts, memoryMode)) {
    Serial.println(F("Allocation failed: CoreMark pass skipped."));
    completedRuns++;
    return false;
  }

  if (kind == SINGLE_SRAM) Serial.println(F("\n--- 1 CORE / INTERNAL SRAM ---"));
  else if (kind == SINGLE_PSRAM) Serial.println(F("\n--- 1 CORE / PSRAM ---"));
  else if (kind == DUAL_SRAM) Serial.println(F("\n--- 2 CORES / INTERNAL SRAM ---"));
  else if (kind == DUAL_PSRAM) Serial.println(F("\n--- 2 CORES / PSRAM ---"));
  else Serial.println(F("\n--- 2 CORES / SRAM + PSRAM ---"));

  if (single) startLiveOled();
  coremark_main();
  if (single) stopLiveOled();
  bool pass = logValidated && !logError && !coremark_port_failed()
              && logSeconds >= 10.0 && logScore > 0.0 && measuredMhz > 0;
  BenchmarkResult &result = results[kind];
  result.pass = pass;
  result.coremarksPerSecond = logScore;
  result.coremarksPerMhz = measuredMhz > 0 ? logScore / measuredMhz : 0.0;
  result.seconds = logSeconds;
  coremark_release_run();
  completedRuns++;

  Serial.printf("Pass: %s, %.3f CoreMark/s, %.4f CoreMark/MHz, %.2f s\n",
                pass ? "PASS" : "FAIL", result.coremarksPerSecond,
                result.coremarksPerMhz, result.seconds);
  return pass;
}

void setup() {
  Serial.begin(115200);
  delay(800);

  if (ESP.getChipCores() != 2) {
    Serial.println(F("ERROR: This sketch needs an ESP32 with 2 CPU cores."));
    return;
  }

  if (ENABLE_OLED) {
    Wire.begin(OLED_SDA, OLED_SCL);
    Wire.setClock(400000);
    Wire.beginTransmission(OLED_ADDRESS);
    if (Wire.endTransmission() == 0 && oled.begin(OLED_ADDRESS, true)) {
      oledReady = true;
      oled.clearDisplay();
      oled.display();
    } else {
      Serial.println(F("OLED not found at 0x3C; continuing on Serial only."));
    }
  }

  psramAvailable = ESP.getPsramSize() >= 2 * 2000;
  totalRuns = psramAvailable ? 5 : 2;
  measuredMhz = ESP.getCpuFreqMHz();
  snprintf(oledChipLine, sizeof(oledChipLine), "%s %luMHz",
           ESP.getChipModel(), (unsigned long)measuredMhz);
  snprintf(oledMemoryLine, sizeof(oledMemoryLine), "F%luM P%luM  0x3C",
           (unsigned long)(ESP.getFlashChipSize() / (1024UL * 1024UL)),
           (unsigned long)(ESP.getPsramSize() / (1024UL * 1024UL)));
  snprintf(oledFinalLine, sizeof(oledFinalLine), "F%lu P%lu CoreMark/MHz",
           (unsigned long)(ESP.getFlashChipSize() / (1024UL * 1024UL)),
           (unsigned long)(ESP.getPsramSize() / (1024UL * 1024UL)));
  printModuleInfo();

  // Both workers can starve the idle tasks for >10 seconds. Extend TWDT's
  // timeout for this run without unsubscribing either idle task/hook.
  esp_task_wdt_config_t normalWdt = normalWatchdogConfig();
  esp_task_wdt_config_t benchmarkWdt = normalWdt;
  benchmarkWdt.timeout_ms = BENCHMARK_WDT_TIMEOUT_MS;
  esp_err_t wdtResult = esp_task_wdt_reconfigure(&benchmarkWdt);
  bool restoreWdt = wdtResult == ESP_OK;
  if (wdtResult != ESP_OK && wdtResult != ESP_ERR_INVALID_STATE) {
    Serial.printf("ERROR: Cannot extend task watchdog: %s\n", esp_err_to_name(wdtResult));
    Serial.println(F("Benchmark not started to avoid a watchdog reset."));
    return;
  }
  if (restoreWdt) Serial.println(F("Task watchdog timeout: 120 s during benchmark."));

  if (oledReady) {
    oledTaskExited = xSemaphoreCreateBinary();
    if (oledTaskExited == nullptr) {
      Serial.println(F("OLED live progress unavailable; static screens remain enabled."));
    }
  }

  runCoremarkPass(SINGLE_SRAM);
  if (psramAvailable) runCoremarkPass(SINGLE_PSRAM);
  runCoremarkPass(DUAL_SRAM);
  if (psramAvailable) runCoremarkPass(DUAL_PSRAM);
  if (psramAvailable) runCoremarkPass(DUAL_HYBRID);

  __atomic_store_n(&currentRun, static_cast<uint8_t>(FINISHED), __ATOMIC_RELEASE);
  drawFinalScreen();
  if (restoreWdt) {
    delay(20);  // Let the idle tasks run once before restoring the short timeout.
    esp_err_t restored = esp_task_wdt_reconfigure(&normalWdt);
    if (restored != ESP_OK) {
      Serial.printf("WARNING: Cannot restore task watchdog: %s\n",
                    esp_err_to_name(restored));
    }
  }

  Serial.println(F("\n=== SUMMARY ==="));
  const char *labels[5] = {"1C SRAM", "1C PSRAM", "2C SRAM",
                           "2C PSRAM", "2C SRAM+PSRAM"};
  for (uint8_t i = 0; i < 5; ++i) {
    if (!psramAvailable && (i == SINGLE_PSRAM || i == DUAL_PSRAM ||
                            i == DUAL_HYBRID)) {
      Serial.printf("%-14s N/A (PSRAM not detected/enabled)\n", labels[i]);
      continue;
    }
    Serial.printf("%-14s %s  %.3f CoreMark/s  %.4f CoreMark/MHz\n",
                  labels[i], results[i].pass ? "PASS" : "FAIL",
                  results[i].coremarksPerSecond, results[i].coremarksPerMhz);
  }
  if (results[SINGLE_SRAM].pass && results[DUAL_SRAM].pass) {
    Serial.printf("SRAM 2-core scaling: %.3fx\n",
                  results[DUAL_SRAM].coremarksPerSecond /
                  results[SINGLE_SRAM].coremarksPerSecond);
  }
  Serial.println(F("Reset the board to run again."));
}

void loop() {
  delay(1000);
}

