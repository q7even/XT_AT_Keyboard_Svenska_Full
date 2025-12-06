#ifndef KEYMAP_H
#define KEYMAP_H

#include <Arduino.h>

/*
  keymap.h

  Default USB HID -> XT/AT scancode mapping (256 entries).
  This map follows the earlier usb_to_xt[] used in the project.
  You may edit this table or load keymaps from web UI.
*/

extern const uint8_t default_usb_to_xt[256];

#endif // KEYMAP_H
