#ifndef KEYMAP_MANAGER_H
#define KEYMAP_MANAGER_H

#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <stdint.h>

// Initialize keymap manager (loads from FS or defaults).
// Pass server pointer to register HTTP endpoints (can be nullptr).
void keymapInit(AsyncWebServer *server = nullptr);

// Save current in-memory keymap to LittleFS and persist via configSave()
bool keymapSave();

// Load keymap from LittleFS (returns true if successful)
bool keymapLoadFromFS();

// Reset keymap to built-in defaults (default_usb_to_xt from keymap.cpp)
void keymapResetToDefault();

// Return keymap as JSON string (array of 256 ints)
String getKeymapJSON();

// Set a single mapping: usb -> xt (both 0..255)
bool setKeymapEntry(uint8_t usbCode, uint8_t xtCode);

// Get mapped XT code for a given USB HID code (returns 0x00 if unmapped)
uint8_t mapHIDtoXT(uint8_t hidCode);

// Endpoint helper: register endpoints on provided server (if not using keymapInit(server))
void registerKeymapEndpoints(AsyncWebServer *server);

#endif // KEYMAP_MANAGER_H
