#include <Arduino.h>
#include <WiFi.h>

const char *encryptionName(wifi_auth_mode_t mode) {
  switch (mode) {
    case WIFI_AUTH_OPEN: return "OPEN";
    case WIFI_AUTH_WEP: return "WEP";
    case WIFI_AUTH_WPA_PSK: return "WPA";
    case WIFI_AUTH_WPA2_PSK: return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/WPA2";
    case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-EAP";
    case WIFI_AUTH_WPA3_PSK: return "WPA3";
    case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA2/WPA3";
    default: return "OTHER";
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  Serial.println("\n=== WIFI SCAN ===");
}

void loop() {
  Serial.println("Dang quet...");
  int count = WiFi.scanNetworks(false, true);

  if (count < 0) {
    Serial.printf("Scan error: %d\n", count);
  } else if (count == 0) {
    Serial.println("Khong tim thay mang nao.");
  } else {
    Serial.printf("Tim thay %d mang:\n", count);
    for (int i = 0; i < count; ++i) {
      Serial.printf("%2d | ch %2d | %4d dBm | %-10s | %s\n",
                    i + 1,
                    WiFi.channel(i),
                    WiFi.RSSI(i),
                    encryptionName(WiFi.encryptionType(i)),
                    WiFi.SSID(i).c_str());
    }
  }

  WiFi.scanDelete();
  Serial.println();
  delay(5000);
}

