#pragma once
#include <Arduino.h>

void xtatBegin(uint8_t clkPin, uint8_t dataPin, unsigned int bitDelayUs);
void xtatTask();
void xtatSendFromUSB(uint8_t hidcode, bool pressed);
void xt_send_make(uint8_t scancode);
void xt_send_break_code(uint8_t scancode);

extern bool xtat_debug_enabled;
extern bool xtat_timestamp_enabled;
extern bool xtat_bitdump_enabled;
extern bool xtat_hostecho_enabled;

size_t xtatPopHostEcho(uint8_t *buf, size_t max);
