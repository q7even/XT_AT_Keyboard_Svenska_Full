#include "realtime_ws.h"
#include <Arduino.h>

static AsyncWebSocket *ws = nullptr;
static AsyncWebServer *gserver = nullptr;

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
    }
  });
  Serial.println("[WS] /ws/scancodes ready");
}

void realtimeBroadcastScancode(const String &msg) {
  if (!ws) return;
  ws->textAll(msg);
}
