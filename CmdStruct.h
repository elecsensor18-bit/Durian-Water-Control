#pragma once
#include <Arduino.h>

enum class ControlMode : uint8_t { MANUAL=0, MINMAX=1, TIMER=2, TIMER_MINMAX=3 };

struct TimerWindow {
  bool     en = false;
  uint8_t  daysMask = 0;       // bit0=Sun ... bit6=Sat
  uint16_t startMin = 65535;   // minute of day
  uint16_t durationMin = 0;
  uint8_t  relayMask = 0;      // mask relays allowed in this window
};

struct CmdStruct {
  String id;
  uint32_t ts = 0;

  ControlMode mode = ControlMode::MANUAL;

  uint8_t targetsMask = 0x0F;  // which relays are controlled
  uint8_t manualMask  = 0;     // manual ON relays (bitmask)

  float minVal = 0;
  float maxVal = 100;
  float hyst   = 0;

  static const uint8_t MAX_WINDOWS = 6;
  TimerWindow windows[MAX_WINDOWS];
  uint8_t windowCount = 0;

  bool failSafeOff = true;     // "OFF"
  uint16_t sensorTimeoutSec = 300;
};
