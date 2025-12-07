#include "detect_protocol.h"
#include "xt_at_output.h"
#include "realtime_ws.h"
#include <Arduino.h>

static void clearHostEchoBuffer() {
  uint8_t tmp[64];
  while (xtatPopHostEcho(tmp, sizeof(tmp)) > 0) { }
}

String detectProtocolSync(unsigned long timeoutMs) {
  unsigned long start = millis();
  clearHostEchoBuffer();
  uint8_t testHID = 4;
  uint8_t xt = config.keymap[testHID] ? config.keymap[testHID] : default_usb_to_xt[testHID];
  if (xt == 0) xt = 0x1C;
  xt_send_make(xt);
  delay(20);
  xt_send_break_code(xt);
  unsigned long deadline = millis() + 300;
  while (millis() < deadline) {
    uint8_t buf[16];
    size_t n = xtatPopHostEcho(buf, sizeof(buf));
    if (n > 0) {
      for (size_t i=0;i<n;i++) {
        if (buf[i] == 0xFA) return "PS2";
      }
      return "PS2";
    }
    delay(20);
  }
  xt_send_make(xt);
  delay(20);
  xt_send_break_code(xt);
  deadline = millis() + 300;
  while (millis() < deadline) {
    uint8_t buf[16];
    size_t n = xtatPopHostEcho(buf, sizeof(buf));
    if (n > 0) return "PS2";
    delay(20);
  }
  return "AT";
}

void detectProtocolAsync(AsyncWebServerRequest *req) {
  String result = detectProtocolSync(2000);
  String json = "{ "suggested":"" + result + "", "note":"heuristic detection; verify in UI" }";
  req->send(200, "application/json", json);
}
