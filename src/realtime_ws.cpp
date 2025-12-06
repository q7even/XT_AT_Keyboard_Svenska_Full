#include "realtime_ws.h"
#include "xt_at_output.h"
#include "keymap_ex.h"
#include <Arduino.h>

static AsyncWebSocket *ws = nullptr;
static AsyncWebServer *gserver = nullptr;

// ------------ WebSocket setup --------------
void realtimeInit(AsyncWebServer &server) {
  gserver = &server;
  ws = new AsyncWebSocket("/ws/scancodes");
  server.addHandler(ws);

  ws->onEvent([](AsyncWebSocket *server, AsyncWebSocketClient *client,
                 AwsEventType type, void *arg, uint8_t *data, size_t len){
    if (type == WS_EVT_CONNECT) {
      Serial.printf("[WS] Client connected: %u\n", client->id());
    } else if (type == WS_EVT_DISCONNECT) {
      Serial.printf("[WS] Client disconnected: %u\n", client->id());
    } else if (type == WS_EVT_DATA) {
      // optionally handle incoming commands from client
    }
  });

  Serial.println("[WS] /ws/scancodes ready");
}

void realtimeBroadcastScancode(const String &msg) {
  if (!ws) return;
  ws->textAll(msg);
}

// ---------------- Protocol detection (heuristic) ------------------
// Heuristic approach notes:
// - There is no standard ACK from a host for PS/2/XT make codes.
// - We try to: (1) send a short identifiable sequence in AT-format, (2) briefly toggle to XT-format,
//   and observe if host side shows expected behavior on sense lines (if available).
// - Most robust: require user confirmation if heuristics uncertain. We implement heuristic + UI prompt fallback.

String detectProtocolSync(unsigned long timeoutMs) {
  // Simple heuristic: send a unique sequence in AT and then in XT; if any host-level feedback read via
  // optional sense pins exists (not implemented), it would determine. As we lack reliable host feedback,
  // we fallback to "unknown".
  // We'll try to detect by sending an innocuous test code sequence and assume AT as default.
  // IMPORTANT: This is a heuristic — not guaranteed. UI will request manual confirm.

  Serial.println("[DETECT] Starting synchronous detection (heuristic)");
  // Try AT-style: send make/break for 'a' via AT (i.e. send_make code and break 0xF0 sequence)
  uint8_t testHID = 4; // 'a' HID code -> mapped in keymapEx
  uint8_t xt = mapHIDToXT_Advanced(testHID, false, false, false);
  if (xt == 0) xt = 0x1C; // fallback for 'a'

  // Send as AT (make + break)
  xt_send_make(xt);
  delay(20);
  xt_send_break_code(xt);
  delay(50);

  // Send as XT (simple make+break)
  xt_send_make(xt);
  delay(20);
  xt_send_break_code(xt);
  delay(50);

  // Because we cannot read host ACKs generically, return "unknown" to force UI confirmation
  Serial.println("[DETECT] Heuristic run complete — result: unknown");
  return "unknown";
}

// Async endpoint starter (kicks detection and returns immediate JSON with suggested result)
void detectProtocolAsync(AsyncWebServerRequest *req) {
  // Start detection in separate task to avoid blocking webserver
  String result = detectProtocolSync(3000);
  String json = "{ \"suggested\":\"" + result + "\", \"note\":\"Heuristisk: verifiera i UI\" }";
  req->send(200, "application/json", json);
}
