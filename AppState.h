#pragma once
#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "CmdStruct.h"

struct SensorData {
  float soil = 0.0f;
  float temp = 0.0f;
  int   ec   = 0;
  float airTemp = NAN;
  float airHum  = NAN;
  uint32_t tsMs = 0;
};

struct ControlRuntime {
  String activeMode = "BOOT";
  uint8_t relayMask = 0;
  bool inWindow = false;
  String decision = "OFF"; // ON/OFF/HOLD
  String why = "boot";
};

struct AckInfo {
  String lastCmdId;
  uint32_t appliedAtMs = 0;
  String result = "OK";
  String reason = "applied";
};

class AppState {
public:
  void begin();

    // control report pending (when runtime changes)
  void setControlPending(bool v);
  bool takeControlPending();

  // sensor
  void setSensor(const SensorData& s);
  SensorData getSensor();

  // cmd
  void setPendingCmd(const CmdStruct& c);
  CmdStruct getPendingCmd();

  void setAppliedCmdId(const String& id);
  String getAppliedCmdId();

  void setCmdDirty(bool v);
  bool takeCmdDirty(); // one-shot

  // runtime
  void setRuntime(const ControlRuntime& r);
  ControlRuntime getRuntime();

  // ack
  void setAck(const AckInfo& a);
  AckInfo getAck();

  void setAckPending(bool v);
  bool takeAckPending(); // one-shot

private:
  bool _controlPending = false;

  SemaphoreHandle_t _mtx = nullptr;
  void lock();
  void unlock();

  SensorData _sensor;
  CmdStruct _pendingCmd;
  String _appliedCmdId;
  bool _cmdDirty = false;

  ControlRuntime _rt;
  AckInfo _ack;
  bool _ackPending = false;
};

extern AppState STATE;
