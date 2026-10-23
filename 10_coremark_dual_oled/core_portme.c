/* CoreMark 1.0 Arduino-ESP32 port; EEMBC algorithm sources unchanged. */
#include "coremark.h"
#include <stdbool.h>
#include <stdlib.h>
#include "freertos/task.h"
#include "esp_heap_caps.h"

extern uint32_t Arduino_micros(void);

volatile ee_s32 seed1_volatile = 0;
volatile ee_s32 seed2_volatile = 0;
volatile ee_s32 seed3_volatile = 0x66;
volatile ee_s32 seed4_volatile = ITERATIONS;
volatile ee_s32 seed5_volatile = 0;

ee_u32 default_num_contexts = 1;

typedef struct {
  SemaphoreHandle_t go;
  SemaphoreHandle_t done;
  SemaphoreHandle_t exited;
  core_results *result;
  bool started;
  bool stop;
} worker_slot;

static worker_slot workers[MULTITHREAD];
static void *reserved_blocks[MULTITHREAD];
static uint8_t allocation_index;
static uint8_t launch_index;
static uint8_t active_contexts;
static uint8_t memory_mode;
static bool port_failed;
static bool measurement_active;
static uint32_t measurement_start_ms;
static uint32_t estimated_duration_ms;
static CORE_TICKS start_time_val;
static CORE_TICKS stop_time_val;
static CORE_TICKS calibration_ticks;

const char *coremark_memory_location(void) {
  if (memory_mode == 1) return "all contexts: external PSRAM";
  if (memory_mode == 2) return "context 0: SRAM; context 1: PSRAM";
  return "all contexts: internal SRAM";
}

static void coremark_worker(void *arg) {
  worker_slot *slot = (worker_slot *)arg;
  for (;;) {
    xSemaphoreTake(slot->go, portMAX_DELAY);
    if (slot->stop) break;
    iterate(slot->result);
    xSemaphoreGive(slot->done);
  }
  xSemaphoreGive(slot->exited);
  vTaskDelete(NULL);
}

void coremark_release_run(void) {
  __atomic_store_n(&measurement_active, false, __ATOMIC_RELEASE);
  for (uint8_t i = 0; i < MULTITHREAD; ++i) {
    worker_slot *slot = &workers[i];
    if (slot->started) {
      slot->stop = true;
      xSemaphoreGive(slot->go);
      xSemaphoreTake(slot->exited, portMAX_DELAY);
      slot->started = false;
    }
    if (slot->go != NULL) vSemaphoreDelete(slot->go);
    if (slot->done != NULL) vSemaphoreDelete(slot->done);
    if (slot->exited != NULL) vSemaphoreDelete(slot->exited);
    slot->go = slot->done = slot->exited = NULL;
    if (reserved_blocks[i] != NULL) heap_caps_free(reserved_blocks[i]);
    reserved_blocks[i] = NULL;
  }
}

bool coremark_prepare_run(uint8_t contexts, uint8_t memoryMode) {
  if (contexts < 1 || contexts > MULTITHREAD || memoryMode > 2 ||
      (memoryMode == 2 && contexts != 2)) {
    return false;
  }
  default_num_contexts = contexts;
  active_contexts = contexts;
  memory_mode = memoryMode;
  port_failed = false;
  __atomic_store_n(&measurement_active, false, __ATOMIC_RELEASE);
  __atomic_store_n(&measurement_start_ms, 0, __ATOMIC_RELEASE);
  __atomic_store_n(&estimated_duration_ms, 12000, __ATOMIC_RELEASE);
  calibration_ticks = 0;
  allocation_index = 0;
  launch_index = 0;
  for (uint8_t i = 0; i < MULTITHREAD; ++i) {
    reserved_blocks[i] = NULL;
    workers[i] = (worker_slot){0};
  }

  // CoreMark allocates two blocks even when only one context is timed.
  for (uint8_t i = 0; i < MULTITHREAD; ++i) {
    bool external = memoryMode == 1 || (memoryMode == 2 && i == 1);
    uint32_t caps = MALLOC_CAP_8BIT |
                    (external ? MALLOC_CAP_SPIRAM : MALLOC_CAP_INTERNAL);
    reserved_blocks[i] = heap_caps_malloc(TOTAL_DATA_SIZE, caps);
    if (reserved_blocks[i] == NULL) goto fail;
  }

  // Sleeping pinned workers are created before CoreMark calls start_time().
  for (uint8_t i = 0; i < active_contexts; ++i) {
    worker_slot *slot = &workers[i];
    slot->go = xSemaphoreCreateBinary();
    slot->done = xSemaphoreCreateBinary();
    slot->exited = xSemaphoreCreateBinary();
    if (slot->go == NULL || slot->done == NULL || slot->exited == NULL) goto fail;
    BaseType_t core = (active_contexts == 1) ? 1 : i;
    if (xTaskCreatePinnedToCore(coremark_worker, "coremark", 8192,
                                slot, 2, NULL, core) != pdPASS) goto fail;
    slot->started = true;
  }
  return true;

fail:
  port_failed = true;
  coremark_release_run();
  return false;
}

bool coremark_port_failed(void) { return port_failed; }
bool coremark_measurement_active(void) {
  return __atomic_load_n(&measurement_active, __ATOMIC_ACQUIRE);
}
uint32_t coremark_measurement_start_ms(void) {
  return __atomic_load_n(&measurement_start_ms, __ATOMIC_ACQUIRE);
}
uint32_t coremark_estimated_duration_ms(void) {
  return __atomic_load_n(&estimated_duration_ms, __ATOMIC_ACQUIRE);
}

void *portable_malloc(ee_size_t size) {
  if (size != TOTAL_DATA_SIZE || allocation_index >= MULTITHREAD) {
    port_failed = true;
    return NULL;
  }
  return reserved_blocks[allocation_index++];
}

void portable_free(void *p) {
  for (uint8_t i = 0; i < MULTITHREAD; ++i) {
    if (reserved_blocks[i] == p && p != NULL) {
      heap_caps_free(p);
      reserved_blocks[i] = NULL;
      return;
    }
  }
}

void start_time(void) { start_time_val = Arduino_micros(); }
void stop_time(void) {
  stop_time_val = Arduino_micros();
  if (!__atomic_load_n(&measurement_active, __ATOMIC_ACQUIRE)) {
    calibration_ticks = stop_time_val - start_time_val;
  } else {
    __atomic_store_n(&measurement_active, false, __ATOMIC_RELEASE);
  }
}
CORE_TICKS get_time(void) { return stop_time_val - start_time_val; }
secs_ret time_in_secs(CORE_TICKS ticks) {
  return (secs_ret)ticks / (secs_ret)1000000.0;
}

void portable_init(core_portable *p, int *argc, char *argv[]) {
  (void)argc;
  (void)argv;
  if (sizeof(ee_ptr_int) != sizeof(void *) || sizeof(ee_u32) != 4) {
    port_failed = true;
    ee_printf("ERROR: CoreMark type sizes are wrong.\n");
  }
  p->portable_id = 1;
  p->done = NULL;
}

void portable_fini(core_portable *p) { p->portable_id = 0; }

ee_u8 core_start_parallel(core_results *res) {
  const uint8_t index = launch_index++;
  if (index == 0) {
    uint32_t divisor = calibration_ticks / 1000000UL;
    if (divisor == 0) divisor = 1;
    uint32_t multiplier = 1 + 10 / divisor;
    uint64_t estimate = (uint64_t)calibration_ticks * multiplier / 1000ULL;
    uint32_t estimate_ms = estimate > UINT32_MAX ? UINT32_MAX : (uint32_t)estimate;
    if (estimate_ms == 0) estimate_ms = 12000;
    __atomic_store_n(&estimated_duration_ms, estimate_ms, __ATOMIC_RELEASE);
    __atomic_store_n(&measurement_start_ms, Arduino_micros() / 1000UL,
                     __ATOMIC_RELEASE);
    __atomic_store_n(&measurement_active, true, __ATOMIC_RELEASE);
  }

  if (index >= active_contexts || !workers[index].started) {
    port_failed = true;
    ee_printf("ERROR: CoreMark worker %u is unavailable.\n", index);
    iterate(res);
    return 0;
  }
  workers[index].result = res;
  if (xSemaphoreGive(workers[index].go) != pdTRUE) {
    port_failed = true;
    ee_printf("ERROR: Could not wake CoreMark worker %u.\n", index);
    iterate(res);
    xSemaphoreGive(workers[index].done);
    return 0;
  }
  return 1;
}

ee_u8 core_stop_parallel(core_results *res) {
  for (uint8_t i = 0; i < active_contexts; ++i) {
    if (workers[i].result == res) {
      BaseType_t taken = xSemaphoreTake(workers[i].done, portMAX_DELAY);
      if (taken != pdTRUE) port_failed = true;
      return taken == pdTRUE;
    }
  }
  port_failed = true;
  return 0;
}
