#ifndef KEYMAP_EX_H
#define KEYMAP_EX_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

struct KeymapEntry {
    uint8_t base;
    uint8_t shift;
    uint8_t altgr;
    uint8_t ctrl;
    uint8_t dead;
};

extern KeymapEntry keymapEx[256];

void keymapExInit(AsyncWebServer *server = nullptr);
String keymapExJSON();
bool keymapExSet(uint8_t usb, KeymapEntry e);
bool keymapExLoadFS();
bool keymapExSaveFS();
void keymapExResetDefault();
uint8_t mapHIDToXT_Advanced(uint8_t hid, bool shift, bool altgr, bool ctrl);
void registerKeymapExEndpoints(AsyncWebServer *server);

#endif
