#ifndef KEYMAP_MANAGER_H
#define KEYMAP_MANAGER_H

#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <stdint.h>

void keymapInit(AsyncWebServer *server = nullptr);
bool keymapSave();
bool keymapLoadFromFS();
void keymapResetToDefault();
String getKeymapJSON();
bool setKeymapEntry(uint8_t usbCode, uint8_t xtCode);
uint8_t mapHIDtoXT(uint8_t hidCode);
void registerKeymapEndpoints(AsyncWebServer *server);

#endif
