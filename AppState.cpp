#include "AppState.h"
AppState STATE;

void AppState::begin() {
  _mtx = xSemaphoreCreateMutex();
  _sensor.tsMs = millis();
}

void AppState::lock() {
  xSemaphoreTake(_mtx, portMAX_DELAY);
}
void AppState::unlock() {
  xSemaphoreGive(_mtx);
}

void AppState::setSensor(const SensorData& s) {
  lock();
  _sensor = s;
  unlock();
}
SensorData AppState::getSensor() {
  lock();
  auto s = _sensor;
  unlock();
  return s;
}

void AppState::setPendingCmd(const CmdStruct& c) {
  lock();
  _pendingCmd = c;
  unlock();
}
CmdStruct AppState::getPendingCmd() {
  lock();
  auto c = _pendingCmd;
  unlock();
  return c;
}

void AppState::setAppliedCmdId(const String& id) {
  lock();
  _appliedCmdId = id;
  unlock();
}
String AppState::getAppliedCmdId() {
  lock();
  auto s = _appliedCmdId;
  unlock();
  return s;
}

void AppState::setCmdDirty(bool v) {
  lock();
  _cmdDirty = v;
  unlock();
}
bool AppState::takeCmdDirty() {
  lock();
  bool v = _cmdDirty;
  _cmdDirty = false;
  unlock();
  return v;
}

void AppState::setRuntime(const ControlRuntime& r) {
  lock();
  _rt = r;
  unlock();
}
ControlRuntime AppState::getRuntime() {
  lock();
  auto r = _rt;
  unlock();
  return r;
}

void AppState::setAck(const AckInfo& a) {
  lock();
  _ack = a;
  unlock();
}
AckInfo AppState::getAck() {
  lock();
  auto a = _ack;
  unlock();
  return a;
}

void AppState::setAckPending(bool v) {
  lock();
  _ackPending = v;
  unlock();
}
bool AppState::takeAckPending() {
  lock();
  bool v = _ackPending;
  _ackPending = false;
  unlock();
  return v;
}

void AppState::setControlPending(bool v) {
  lock();
  _controlPending = v;
  unlock();
}
bool AppState::takeControlPending() {
  lock();
  bool v = _controlPending;
  _controlPending = false;
  unlock();
  return v;
}
