#include <Arduino.h>
#include "wifi_manager.h"
#include "webui.h"
#include "config.h"
#include "ota.h"
#include "usb_host.h"
#include "xt_at_output.h"

void setup() {
    Serial.begin(115200);
    delay(300);

    configLoad();

    wifiStart();               // AP + STA
    webuiStart();              // Webserver + UI
    setupOTA();                // /update
    usbHostBegin();            // USB → HID keyboard
    xtatBegin();               // XT/AT output

    Serial.println("System Online.");
}

void loop() {
    usbHostTask();         // USB events
    xtatTask();            // Send scancodes
}
