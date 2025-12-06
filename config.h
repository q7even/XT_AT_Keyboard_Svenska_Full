#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

/*
  config.h
  Central application configuration structure and forward declarations.
  Persistent storage handled via Preferences in config.cpp.
*/

#define DEFAULT_AP_SSID "XT-Keyboard"
#define DEFAULT_AP_PASS "12345678"

// Modes
#define MODE_XT 1
#define MODE_AT 2
#define MODE_PS2 3

struct AppConfig {
  // WiFi
  String ap_ssid;
  String ap_pass;
  String sta_ssid;
  String sta_pass;

  // Protocol mode (XT/AT/PS2)
  uint8_t kb_mode;

  // OTA
  String ota_user;
  String ota_pass;
  String ota_manifest_url;

  // Keymap (256 bytes, mapping USB HID code -> XT code)
  uint8_t keymap[256];

  // Extra flags
  bool auto_ota_enabled;
  unsigned long auto_ota_interval_ms;
};

extern AppConfig config;

// Config I/O
void configLoad();   // load from NVS
void configSave();   // save to NVS
void configFactoryReset(); // reset defaults to flash

#endif // CONFIG_H
