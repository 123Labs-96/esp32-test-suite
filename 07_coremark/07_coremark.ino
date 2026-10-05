// CoreMark 1.0 benchmark for Arduino-ESP32.
//
// The benchmark algorithms in this folder come unchanged from EEMBC CoreMark.
// This file and core_portme.* are only the Arduino-ESP32 platform port.

#include <Arduino.h>
#include <stdarg.h>
#include <stdint.h>

#if __has_include(<esp_arduino_version.h>)
#include <esp_arduino_version.h>
#endif

extern "C" int coremark_main(void);

void setup()
{
  Serial.begin(115200);

  // Native USB boards may need a moment for Serial Monitor, but do not wait
  // forever because many ESP32 boards use a normal UART bridge.
  const uint32_t serialWaitStart = millis();
  while (!Serial && (millis() - serialWaitStart < 3000)) {
    delay(10);
  }

  delay(500);
  Serial.println();
  Serial.println("=== EEMBC COREMARK 1.0 / ARDUINO-ESP32 ===");
  Serial.printf("Chip          : %s rev %d\n",
                ESP.getChipModel(), ESP.getChipRevision());
  Serial.printf("CPU           : %u MHz, %d core(s)\n",
                ESP.getCpuFreqMHz(), ESP.getChipCores());
#if defined(ESP_ARDUINO_VERSION_STR)
  Serial.printf("Arduino core  : %s\n", ESP_ARDUINO_VERSION_STR);
#endif
  Serial.printf("Free heap     : %u bytes\n", ESP.getFreeHeap());
  Serial.println("Mode          : 1 context, 2K performance run");
  Serial.println("Score         : Iterations/Sec (higher is better)");
  Serial.println("Please wait; a valid reported run must last at least 10 seconds.");
  Serial.println();

  delay(250);
  coremark_main();

  Serial.println();
  Serial.println("CoreMark finished. Press RESET to run again.");
}

void loop()
{
  delay(1000);
}

// CoreMark uses this callback to print its result through Arduino Serial.
extern "C" int ee_printf(const char *format, ...)
{
  va_list args;
  va_start(args, format);

  for (; *format; ++format) {
    if (*format != '%') {
      if (*format == '\n') Serial.print('\r');
      Serial.print(*format);
      continue;
    }

    bool isLong = false;
    ++format;

    if (*format == '%') {
      Serial.print('%');
      continue;
    }

    if (*format == '-') ++format;
    while (*format >= '0' && *format <= '9') ++format;
    if (*format == 'l') {
      isLong = true;
      ++format;
    }
    if (*format == '\0') break;

    switch (*format) {
      case 's':
        Serial.print(va_arg(args, const char *));
        break;
      case 'f':
        Serial.print(va_arg(args, double), 6);
        break;
      case 'd':
        if (isLong) Serial.print(va_arg(args, long));
        else Serial.print(va_arg(args, int));
        break;
      case 'u':
        if (isLong) Serial.print(va_arg(args, unsigned long));
        else Serial.print(va_arg(args, unsigned int));
        break;
      case 'x':
        if (isLong) Serial.print(va_arg(args, unsigned long), HEX);
        else Serial.print(va_arg(args, unsigned int), HEX);
        break;
      case 'c':
        Serial.print((char)va_arg(args, int));
        break;
      default:
        Serial.print('%');
        Serial.print(*format);
        break;
    }
  }

  va_end(args);
  return 1;
}

// CoreMark timing callback. micros() is sufficient because a standard run is
// only tens of seconds and unsigned subtraction handles timer wraparound.
extern "C" uint32_t Arduino_micros(void)
{
  return micros();
}
