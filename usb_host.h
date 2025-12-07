#ifndef USB_HOST_H
#define USB_HOST_H

#include <Arduino.h>

void usbHostBegin();
void usbHostTask();

typedef void (*usb_key_cb_t)(uint8_t hidcode, bool pressed);
void usbHostRegisterKeyCallback(usb_key_cb_t cb);

#endif
