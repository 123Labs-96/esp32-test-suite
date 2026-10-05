/*
Copyright 2018 Embedded Microprocessor Benchmark Consortium (EEMBC)

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0
*/

#include "coremark.h"
#include "core_portme.h"

extern uint32_t Arduino_micros(void);

volatile ee_s32 seed1_volatile = 0x0;
volatile ee_s32 seed2_volatile = 0x0;
volatile ee_s32 seed3_volatile = 0x66;
volatile ee_s32 seed4_volatile = ITERATIONS;
volatile ee_s32 seed5_volatile = 0;

#define EE_TICKS_PER_SEC 1000000.0

static CORE_TICKS start_time_val;
static CORE_TICKS stop_time_val;

CORETIMETYPE barebones_clock(void)
{
  return Arduino_micros();
}

void start_time(void)
{
  start_time_val = barebones_clock();
}

void stop_time(void)
{
  stop_time_val = barebones_clock();
}

CORE_TICKS get_time(void)
{
  return (CORE_TICKS)(stop_time_val - start_time_val);
}

secs_ret time_in_secs(CORE_TICKS ticks)
{
  return ((secs_ret)ticks) / (secs_ret)EE_TICKS_PER_SEC;
}

ee_u32 default_num_contexts = 1;

void portable_init(core_portable *p, int *argc, char *argv[])
{
  (void)argc;
  (void)argv;

  if (sizeof(ee_ptr_int) != sizeof(ee_u8 *)) {
    ee_printf("ERROR: ee_ptr_int cannot hold a pointer.\n");
  }
  if (sizeof(ee_u32) != 4) {
    ee_printf("ERROR: ee_u32 is not 32 bits.\n");
  }
  p->portable_id = 1;
}

void portable_fini(core_portable *p)
{
  p->portable_id = 0;
}
