#pragma once
#include <Arduino.h>
#include <Wire.h>

#include <MCP23008.h>
#include <MCP342x.h>
#include <ModbusMaster.h>
#include <RTClib.h>

#include "HS_BoardConfig.h"

/* =========================================================
 *  Beeper
 * ========================================================= */
class Beeper {
public:
  Beeper();
  void begin();
  void on();
  void off();
  void beep(uint16_t durationMs);
};
extern Beeper beeper;

/* =========================================================
 *  LED (MCP23008)
 * ========================================================= */
#ifndef LED_COUNT
#define LED_COUNT 8
#endif

class Led {
public:
  Led(uint8_t index = 0, bool activeLow = false);
  void begin();
  void on();
  void off();
  void toggle();

private:
  uint8_t _pin;
  bool _activeLow;
};
extern Led led[LED_COUNT];

/* =========================================================
 *  Relay
 * ========================================================= */
class Relay {
public:
  Relay();
  void begin();
  void on(uint8_t ch);
  void off(uint8_t ch);
  void toggle(uint8_t ch);
  bool get(uint8_t ch) const;

private:
  bool _status[RELAY_COUNT];
};
extern Relay relay;

/* =========================================================
 *  Switch
 * ========================================================= */
class Switch {
public:
  explicit Switch(uint8_t swPin = 0);
  void begin();
  bool isPressed();
private:
  uint8_t _sw;
};
extern Switch sw[SW_COUNT];

/* =========================================================
 *  ADC (MCP342x)
 * ========================================================= */
class HS_ADC {
public:
  HS_ADC();

  bool begin(uint32_t i2cClockHz = HS_I2C_CLOCK_HZ,
             bool doReset = true,
             uint16_t resetDelayMs = 1);

  bool readRaw(uint8_t ch, long &raw, MCP342x::Config *outStatus = nullptr, unsigned long timeout = 100000UL);
  bool readVoltage(uint8_t ch, float &v_adc, unsigned long timeout = 100000UL);

  // เผื่อใช้วัดแบตผ่าน divider
  bool readBatteryVoltage(uint8_t ch, float &v_batt, unsigned long timeout = 100000UL);

  void setDivider(float r1, float r2) { _r1 = r1; _r2 = r2; }
  void setVref(float vref) { _vref = vref; }

private:
  uint8_t _addr;
  MCP342x _adc;
  bool _ok;

  float _vref;
  float _r1;
  float _r2;

  MCP342x::Mode _mode;
  MCP342x::Resolution _res;
  MCP342x::Gain _gain;

  MCP342x::Channel toChannelEnum(uint8_t ch);
  long fullScaleCounts() const;
};
extern HS_ADC adc;

/* =========================================================
 *  HS_Modbus (รองรับทั้งแบบมี/ไม่มี DE/RE)
 *  - กรณี MAX13487 auto-direction => dePin = -1 และจะไม่แตะ pin ใดๆ
 * ========================================================= */
class HS_Modbus {
public:
  HS_Modbus(HardwareSerial &uart, int rxPin, int txPin, int dePin = -1);

  void begin(uint32_t baud = 9600, SerialConfig serialConfig = SERIAL_8N1);

  bool readRegisters(uint8_t deviceId,
                     uint16_t regAddr,
                     uint16_t regCount,
                     uint16_t *outRegs,
                     uint8_t maxOut);

private:
  HardwareSerial &_uart;
  ModbusMaster _mb;
  int _rxPin, _txPin, _dePin;
  int _baud;
};

/* =========================================================
 *  ModbusSensor
 * ========================================================= */
class ModbusSensor {
public:
  enum DataType { DT_UINT16, DT_INT16, DT_UINT32, DT_INT32, DT_FLOAT32 };

  ModbusSensor(HS_Modbus *modbus,
               uint8_t deviceId,
               uint16_t regAddr,
               uint16_t regCount,
               DataType dtype);

  void begin();
  bool read();
  float get() const;
  uint32_t lastTimestamp() const;

private:
  HS_Modbus *_modbus;
  uint8_t _deviceId;
  uint16_t _regAddr;
  uint16_t _regCount;
  DataType _dtype;

  float _lastValue;
  uint32_t _lastTs;

  float decode(uint16_t *regs, uint8_t count);
};

/* =========================================================
 *  RTC (DS1307 via RTClib)
 * ========================================================= */
class HS_RTC {
public:
  bool begin();
  bool isRunning();
  time_t now();
  void set(time_t t);
  void getDateTime(struct tm &out);
};
extern HS_RTC rtc;
