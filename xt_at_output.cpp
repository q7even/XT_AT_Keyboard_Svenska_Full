// xt_at_output.cpp  (ERSÄTT befintlig)
#include "xt_at_output.h"
#include "keymap.h"
#include "config.h"
#include "realtime_ws.h"
#include <Arduino.h>

// Debug flags (extern defined in header)
bool xtat_debug_enabled     = false;
bool xtat_timestamp_enabled = false;
bool xtat_bitdump_enabled   = false;
bool xtat_hostecho_enabled  = false;

// Hardware defaults
static uint8_t XT_CLK_PIN = 10;
static uint8_t XT_DATA_PIN = 11;
static unsigned int BIT_DELAY_US = 30;

// Queue for outgoing scancodes
#define XT_QUEUE_LEN 32
struct XT_Queued { uint8_t code; bool isBreak; };
static XT_Queued xtQueue[XT_QUEUE_LEN];
static volatile int q_head = 0, q_tail = 0;

static bool q_push(uint8_t code, bool isBreak) {
  int next = (q_tail + 1) % XT_QUEUE_LEN;
  if (next == q_head) return false;
  xtQueue[q_tail].code = code;
  xtQueue[q_tail].isBreak = isBreak;
  q_tail = next;
  return true;
}
static bool q_pop(XT_Queued &out) {
  if (q_head == q_tail) return false;
  out = xtQueue[q_head];
  q_head = (q_head + 1) % XT_QUEUE_LEN;
  return true;
}

// ---------------- Host-echo ring buffer (for detect module) --------------
#define HOST_ECHO_LEN 64
static volatile uint8_t hostEchoBuf[HOST_ECHO_LEN];
static volatile int hostEchoHead = 0, hostEchoTail = 0;

static void hostEchoPush(uint8_t b) {
  int next = (hostEchoTail + 1) % HOST_ECHO_LEN;
  if (next == hostEchoHead) {
    // full -> drop oldest
    hostEchoHead = (hostEchoHead + 1) % HOST_ECHO_LEN;
  }
  hostEchoBuf[hostEchoTail] = b;
  hostEchoTail = next;
}

// Pop up to max elements into buf, return count
size_t xtatPopHostEcho(uint8_t *buf, size_t max) {
  size_t cnt = 0;
  while (hostEchoHead != hostEchoTail && cnt < max) {
    buf[cnt++] = hostEchoBuf[hostEchoHead];
    hostEchoHead = (hostEchoHead + 1) % HOST_ECHO_LEN;
  }
  return cnt;
}

// ---------------- Low-level I/O helpers ----------------
static inline void data_drive_low() {
  pinMode(XT_DATA_PIN, OUTPUT);
  digitalWrite(XT_DATA_PIN, LOW);
}
static inline void data_release_drive_high() {
  pinMode(XT_DATA_PIN, OUTPUT);
  digitalWrite(XT_DATA_PIN, HIGH);
}
static inline void data_release_input() {
  pinMode(XT_DATA_PIN, INPUT_PULLUP);
}
static inline void clk_pulse_and_broadcast() {
  // Broadcast clock falling & rising with data state for oscilloskop
  // Before pulling clock low, sample data
  int dataState = digitalRead(XT_DATA_PIN);
  unsigned long ts = (unsigned long)micros();
  // send JSON for pre-pulse
  {
    char buf[128];
    snprintf(buf, sizeof(buf), "{\"type\":\"bit\",\"phase\":\"pre\",\"ts\":%lu,\"clk\":1,\"data\":%d}", ts, dataState);
    realtimeBroadcastScancode(String(buf));
  }
  digitalWrite(XT_CLK_PIN, LOW);
  delayMicroseconds(BIT_DELAY_US);
  // mid pulse
  dataState = digitalRead(XT_DATA_PIN);
  ts = (unsigned long)micros();
  {
    char buf[128];
    snprintf(buf, sizeof(buf), "{\"type\":\"bit\",\"phase\":\"low\",\"ts\":%lu,\"clk\":0,\"data\":%d}", ts, dataState);
    realtimeBroadcastScancode(String(buf));
  }
  digitalWrite(XT_CLK_PIN, HIGH);
  delayMicroseconds(BIT_DELAY_US);
  // after pulse, broadcast high
  dataState = digitalRead(XT_DATA_PIN);
  ts = (unsigned long)micros();
  {
    char buf[128];
    snprintf(buf, sizeof(buf), "{\"type\":\"bit\",\"phase\":\"high\",\"ts\":%lu,\"clk\":1,\"data\":%d}", ts, dataState);
    realtimeBroadcastScancode(String(buf));
  }
}

// bitdump helper (serial)
static void bitdump_byte_serial(uint8_t b) {
  if (!xtat_bitdump_enabled) return;
  Serial.print("BITDUMP: ");
  for (int i=0;i<8;i++){ Serial.print((b & 1) ? '1':'0'); b >>= 1; }
  Serial.println();
}

// debug JSON to Serial
static void debug_json_serial(const char *type, uint8_t sc) {
  if (!xtat_debug_enabled) return;
  const char *proto = (config.kb_mode==MODE_AT)?"AT":(config.kb_mode==MODE_PS2?"PS2":"XT");
  if (xtat_timestamp_enabled) Serial.printf("{\"ts\":%lu,\"type\":\"%s\",\"code\":\"%02X\",\"proto\":\"%s\"}\n", (unsigned long)micros(), type, sc, proto);
  else Serial.printf("{\"type\":\"%s\",\"code\":\"%02X\",\"proto\":\"%s\"}\n", type, sc, proto);
}

// ws-event wrapper
static void ws_send_json(const char *type, uint8_t sc) {
  char buf[128];
  const char *proto = (config.kb_mode==MODE_AT)?"AT":(config.kb_mode==MODE_PS2?"PS2":"XT");
  if (xtat_timestamp_enabled) snprintf(buf,sizeof(buf), "{\"ts\":%lu,\"type\":\"%s\",\"code\":\"%02X\",\"proto\":\"%s\"}", (unsigned long)micros(), type, sc, proto);
  else snprintf(buf,sizeof(buf), "{\"type\":\"%s\",\"code\":\"%02X\",\"proto\":\"%s\"}", type, sc, proto);
  realtimeBroadcastScancode(String(buf));
}

// ---------------- send raw byte LSB-first ----------------
static void send_byte_raw(uint8_t b) {
  bitdump_byte_serial(b);

  // start bit
  data_drive_low();
  delayMicroseconds(BIT_DELAY_US);

  for (int i=0;i<8;i++) {
    if (b & 0x01) data_release_drive_high(); else data_drive_low();
    clk_pulse_and_broadcast();
    b >>= 1;
  }

  // stop bit
  data_release_drive_high();
  clk_pulse_and_broadcast();
}

// ---------------- Host->Device read (PS/2 host commands) --------------
static bool try_read_host_byte(uint8_t &out) {
  if (!xtat_hostecho_enabled) return false;
  // We expect host uses clock line to send bits. We sample when clock falls.
  // Set pins as input pullups
  pinMode(XT_CLK_PIN, INPUT_PULLUP);
  pinMode(XT_DATA_PIN, INPUT_PULLUP);
  // wait for start bit (clock low + data low) with timeout
  unsigned long start = micros();
  while (digitalRead(XT_CLK_PIN) == HIGH) {
    if (micros() - start > 2000u) return false; // no host activity
  }
  // Now read 8 data bits (LSB first) synchronized to clock transitions
  uint8_t val = 0;
  for (int i=0;i<8;i++) {
    // wait for rising edge
    while (digitalRead(XT_CLK_PIN) == LOW) {
      if (micros() - start > 5000u) return false;
    }
    // sample data
    int d = digitalRead(XT_DATA_PIN);
    val |= (d ? 1 : 0) << i;
    // wait for falling edge
    while (digitalRead(XT_CLK_PIN) == HIGH) {
      if (micros() - start > 5000u) return false;
    }
  }
  // stop bit not strictly checked
  out = val;
  // push into host-echo buffer
  hostEchoPush(val);
  // restore pins to output-high idle
  pinMode(XT_DATA_PIN, OUTPUT); digitalWrite(XT_DATA_PIN, HIGH);
  pinMode(XT_CLK_PIN, OUTPUT); digitalWrite(XT_CLK_PIN, HIGH);
  return true;
}

// Public: pop host echo (declared in header)
size_t xtatPopHostEcho(uint8_t *buf, size_t max) {
  return ::xtatPopHostEcho(buf, max); // forward to internal function (implemented above)
}

// ---------------- High-level make/break ----------------
void xt_send_make(uint8_t scancode) {
  debug_json_serial("make", scancode);
  ws_send_json("make", scancode);
  send_byte_raw(scancode);
}

void xt_send_break_code(uint8_t scancode) {
  if (config.kb_mode == MODE_AT) {
    debug_json_serial("break", 0xF0);
    ws_send_json("break", 0xF0);
    send_byte_raw(0xF0);
    delayMicroseconds(BIT_DELAY_US);
    debug_json_serial("break", scancode);
    ws_send_json("break", scancode);
    send_byte_raw(scancode);
  } else {
    debug_json_serial("break", scancode);
    ws_send_json("break", scancode);
    send_byte_raw(scancode);
  }
}

// Public wrapper for USB host
void xtatSendFromUSB(uint8_t hidcode, bool pressed) {
  uint8_t xtcode = config.keymap[hidcode] ? config.keymap[hidcode] : default_usb_to_xt[hidcode];
  if (!xtcode) return;
  if (!q_push(xtcode, !pressed)) {
    if (pressed) xt_send_make(xtcode);
    else xt_send_break_code(xtcode);
  }
}

void xtatBegin(uint8_t clkPin, uint8_t dataPin, unsigned int bitDelayUs) {
  XT_CLK_PIN = clkPin; XT_DATA_PIN = dataPin; BIT_DELAY_US = bitDelayUs;
  pinMode(XT_CLK_PIN, OUTPUT); digitalWrite(XT_CLK_PIN, HIGH);
  pinMode(XT_DATA_PIN, OUTPUT); digitalWrite(XT_DATA_PIN, HIGH);
  q_head = q_tail = 0;
  hostEchoHead = hostEchoTail = 0;
  Serial.printf("[XT_AT] init CLK=%d DATA=%d delay=%uus\n", XT_CLK_PIN, XT_DATA_PIN, BIT_DELAY_US);
}

void xtatTask() {
  XT_Queued t;
  int processed = 0;
  while (processed < 6 && q_pop(t)) {
    if (!t.isBreak) xt_send_make(t.code);
    else xt_send_break_code(t.code);
    processed++;
  }

  // Attempt to read host bytes non-blocking if hostecho enabled
  if (xtat_hostecho_enabled) {
    uint8_t hv;
    // try a few reads quickly
    int tries = 2;
    while (tries-- > 0) {
      if (try_read_host_byte(hv)) {
        // already queued into host echo buffer inside try_read_host_byte
        // Also broadcast host->dev event
        char buf[64];
        snprintf(buf, sizeof(buf), "{\"type\":\"host->dev\",\"code\":\"%02X\"}", hv);
        realtimeBroadcastScancode(String(buf));
      } else break;
    }
  }
}
