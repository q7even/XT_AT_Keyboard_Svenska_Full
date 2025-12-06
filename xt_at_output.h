#pragma once
#include <Arduino.h>

// ---------------------------------------------------------
// XT/AT/PS2 keyboard output module
// Utökad för:
//   • Serial-debug
//   • Timestamps
//   • Raw bit-dump mode
//   • Host echo analyzer (för PS/2 & AT)
// ---------------------------------------------------------

void xtatBegin(uint8_t clkPin, uint8_t dataPin, unsigned int bitDelayUs);
void xtatTask();   // ska köras i loop()

void xtatSendFromUSB(uint8_t hidcode, bool pressed);

void xt_send_make(uint8_t scancode);
void xt_send_break_code(uint8_t scancode);

// Debug-lägen (globala)
extern bool xtat_debug_enabled;           // normal JSON debug
extern bool xtat_timestamp_enabled;       // inkluderar micros()
extern bool xtat_bitdump_enabled;         // visar varje bit
extern bool xtat_hostecho_enabled;        // aktiverar PS/2 host-readback

// Funktion som används internt i host-echo mode
void xtatCheckHostEcho();
