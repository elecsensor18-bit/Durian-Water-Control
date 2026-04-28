#pragma once
#include <Arduino.h>
void startSensorTask(bool useRandom=true, uint32_t stackWords=4096, UBaseType_t prio=2);
