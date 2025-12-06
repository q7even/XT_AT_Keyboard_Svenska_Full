#include "usb_host.h"
#include "keymap.h"
#include "xt_at_output.h"
#include <Adafruit_TinyUSB.h> // If you use another TinyUSB wrapper, adapt these includes

// NOTE:
// This file uses the Adafruit TinyUSB host wrapper as an example.
// If you use another library, replace init/callbacks accordingly.

static usb_key_cb_t user_cb = nullptr;

// Use Adafruit TinyUSB host classes (available via Adafruit_TinyUSB library)
Adafruit_USBD_HID_Host hid_host; // HID host wrapper (example)

// Internal callback (called by hid_host wrapper)
static void internal_key_report_cb(uint8_t hidcode, bool pressed) {
  // First, call registered user callback
  if (user_cb) user_cb(hidcode, pressed);

  // Default behavior: forward to XT/AT output engine
  xtatSendFromUSB(hidcode, pressed);
}

// Public ------------------------------------------------
void usbHostRegisterKeyCallback(usb_key_cb_t cb) {
  user_cb = cb;
}

void usbHostBegin() {
  // initialize TinyUSB-host subsystem (adapat to your library)
  TinyUSBHost.begin();
  hid_host.begin();

  // Register callback - Adafruit wrapper provides a method
  // Example: hid_host.onKeyReport(callback)
  // If your wrapper differs, change to fit its API.
  hid_host.onKeyReport([](uint8_t hidcode, bool pressed){
    internal_key_report_cb(hidcode, pressed);
  });

  Serial.println("[USB_HOST] Initialized TinyUSB host (HID)");
}

void usbHostTask() {
  // Polling TinyUSB host events (Adafruit wrapper)
  hid_host.Task();
  // If using another wrapper, call its poll loop here.
}
