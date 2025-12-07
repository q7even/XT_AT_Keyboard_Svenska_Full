#include "config.h"
#include <Preferences.h>

static Preferences prefs;

AppConfig config;

void setDefaults() {
  config.ap_ssid = DEFAULT_AP_SSID;
  config.ap_pass = DEFAULT_AP_PASS;
  config.sta_ssid = "";
  config.sta_pass = "";
  config.kb_mode = MODE_AT;
  config.ota_user = "admin";
  config.ota_pass = "keyboard";
  config.ota_manifest_url = "";
  config.auto_ota_enabled = false;
  config.auto_ota_interval_ms = 3600000UL;
  for (int i=0;i<256;i++) config.keymap[i] = 0x00;
}

void configLoad() {
  prefs.begin("cfg", true);
  config.ap_ssid = prefs.getString("ap_ssid", DEFAULT_AP_SSID);
  config.ap_pass = prefs.getString("ap_pass", DEFAULT_AP_PASS);
  config.sta_ssid = prefs.getString("sta_ssid", "");
  config.sta_pass = prefs.getString("sta_pass", "");
  config.kb_mode = prefs.getUInt("kb_mode", MODE_AT);
  config.ota_user = prefs.getString("ota_user", "admin");
  config.ota_pass = prefs.getString("ota_pass", "keyboard");
  config.ota_manifest_url = prefs.getString("ota_manifest_url", "");
  config.auto_ota_enabled = prefs.getBool("auto_ota_enabled", false);
  config.auto_ota_interval_ms = prefs.getULong("auto_ota_interval_ms", 3600000UL);

  size_t len = prefs.getBytesLength("keymap");
  if (len == 256) {
    prefs.getBytes("keymap", config.keymap, 256);
  } else {
    for (int i=0;i<256;i++) config.keymap[i] = 0x00;
  }
  prefs.end();
}

void configSave() {
  prefs.begin("cfg", false);
  prefs.putString("ap_ssid", config.ap_ssid);
  prefs.putString("ap_pass", config.ap_pass);
  prefs.putString("sta_ssid", config.sta_ssid);
  prefs.putString("sta_pass", config.sta_pass);
  prefs.putUInt("kb_mode", config.kb_mode);
  prefs.putString("ota_user", config.ota_user);
  prefs.putString("ota_pass", config.ota_pass);
  prefs.putString("ota_manifest_url", config.ota_manifest_url);
  prefs.putBool("auto_ota_enabled", config.auto_ota_enabled);
  prefs.putULong("auto_ota_interval_ms", config.auto_ota_interval_ms);
  prefs.putBytes("keymap", config.keymap, 256);
  prefs.end();
}

void configFactoryReset() {
  prefs.begin("cfg", false);
  prefs.clear();
  prefs.end();
  setDefaults();
  configSave();
}
