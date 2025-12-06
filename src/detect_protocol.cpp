#include "detect_protocol.h"
#include "xt_at_output.h"
#include "realtime_ws.h"
#include <Arduino.h>

// Heuristic detection approach:
// 1) Clear host-echo buffer
// 2) Send a recognizable test sequence in AT style (0x1C make + F0 break) and observe host->device bytes
// 3) If host responds (any byte) -> likely PS/2 (host sent command) OR host echoed -> consider PS2
// 4) If no host bytes, assume AT/XT. Prefer AT (modern) but mark as "unknown" if uncertain.
// This is heuristic; user confirmation is recommended.

static void clearHostEchoBuffer() {
  uint8_t tmp[64];
  while (xtatPopHostEcho(tmp, sizeof(tmp)) > 0) { /* flush */ }
}

String detectProtocolSync(unsigned long timeoutMs) {
  unsigned long start = millis();
  // flush
  clearHostEchoBuffer();

  // choose test HID 'a' -> mapped scancode
  uint8_t testHID = 4; // 'a'
  uint8_t xt = config.keymap[testHID] ? config.keymap[testHID] : default_usb_to_xt[testHID];
  if (xt == 0) xt = 0x1C; // fallback

  // 1) send AT-style (F0 + code) sequence (press+release)
  xt_send_make(xt);
  delay(20);
  xt_send_break_code(xt);

  // wait briefly for host responses
  unsigned long deadline = millis() + 300;
  while (millis() < deadline) {
    uint8_t buf[16];
    size_t n = xtatPopHostEcho(buf, sizeof(buf));
    if (n > 0) {
      // host sent something -> strong indicator host is PS/2 capable/responding
      // Optionally inspect bytes for 0xFA (ACK) etc.
      for (size_t i=0;i<n;i++) {
        if (buf[i] == 0xFA) {
          return "PS2"; // ack
        }
      }
      return "PS2";
    }
    delay(20);
  }

  // 2) send XT-style (simple make + break) and look again
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

  // No host echo observed: assume AT as modern default; return "AT" but mark heuristic
  return "AT";
}

void detectProtocolAsync(AsyncWebServerRequest *req) {
  // run blocking detection but keep it short
  String result = detectProtocolSync(2000);
  String json = "{ \"suggested\":\"" + result + "\", \"note\":\"heuristic detection; verify in UI\" }";
  req->send(200, "application/json", json);
}
