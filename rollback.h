#ifndef ROLLBACK_H
#define ROLLBACK_H

#include <Arduino.h>

// Initialization: runs early in setup()
void rollbackInitialize();

// Regular loop function
void rollbackPeriodic();

// Returns true if this boot is the first after an OTA flash
bool isFirstBootAfterOTA();

// Force rollback (REST endpoint will use this)
bool rollbackForce();

// Called by OTA module after installation to mark next boot as OTA boot
void markNextBootAsOTA();

#endif
