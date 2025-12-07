#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

#define DEFAULT_AP_SSID "XT-Keyboard"
#define DEFAULT_AP_PASS "12345678"

#define MODE_XT 1
#define MODE_AT 2
#define MODE_PS2 3

struct AppConfig {
  String ap_ssid;
  String ap_pass;
  String sta_ssid;
  String sta_pass;
  uint8_t kb_mode;
  String ota_user;
  String ota_pass;
  String ota_manifest_url;
  uint8_t keymap[256];
  bool auto_ota_enabled;
  unsigned long auto_ota_interval_ms;
};

extern AppConfig config;

void configLoad();
void configSave();
void configFactoryReset();

#endif
