#include "usb_host.h"
#include "xt_at_output.h"
#include <Arduino.h>

// Minimal stub: calls to xtatSendFromUSB should be triggered by actual USB host lib
void usbHostBegin() {
  Serial.println("[USB_HOST] USB host init stub (implement TinyUSB host in usb_host.cpp)");
}

void usbHostTask() {
  // Poll USB host stack here in real implementation
}
void usbHostRegisterKeyCallback(usb_key_cb_t cb) {}
