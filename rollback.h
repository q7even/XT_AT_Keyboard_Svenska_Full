#ifndef ROLLBACK_H
#define ROLLBACK_H

#include <Arduino.h>

void rollbackInitialize();
void rollbackPeriodic();
bool isFirstBootAfterOTA();
bool rollbackForce();
void markNextBootAsOTA();

#endif
