#include "HS_Elecsensor.h"

/* =========================================================
 *  Shared I2C init
 * ========================================================= */
static bool g_i2cInited = false;

static void hsI2CBeginOnce(uint32_t clockHz) {
  if (!g_i2cInited) {
#if defined(ESP32)
    Wire.begin(HS_I2C_SDA, HS_I2C_SCL);
#else
    Wire.begin();
#endif
    Wire.setClock(clockHz);
    g_i2cInited = true;
  }
}

/* =========================================================
 *  MCP23008 shared
 * ========================================================= */
static MCP23008 g_mcp(HS_MCP23008_ADDR);
static bool g_mcpInited = false;

/* =========================================================
 *  Beeper
 * ========================================================= */
Beeper beeper;

Beeper::Beeper() {}

void Beeper::begin() {
  pinMode(BEEP_PIN, OUTPUT);
  digitalWrite(BEEP_PIN, LOW);   // active HIGH buzzer => OFF
}
void Beeper::on()  { digitalWrite(BEEP_PIN, HIGH); }
void Beeper::off() { digitalWrite(BEEP_PIN, LOW);  }
void Beeper::beep(uint16_t durationMs) {
  on();
  delay(durationMs);
  off();
}

/* =========================================================
 *  LED (MCP23008)
 * ========================================================= */
Led led[LED_COUNT] = {
  Led(0), Led(1), Led(2), Led(3),
  Led(4), Led(5), Led(6), Led(7)
};

Led::Led(uint8_t index, bool activeLow)
  : _pin(index), _activeLow(activeLow) {}

void Led::begin() {
  if (!g_mcpInited) {
    hsI2CBeginOnce(HS_I2C_CLOCK_HZ);
    g_mcp.begin();
    g_mcp.pinMode8(0x00);   // all OUTPUT
    g_mcp.write8(0x00);     // all OFF (active HIGH)
    g_mcpInited = true;
  }
  off();
}
void Led::on()  { g_mcp.write1(_pin, _activeLow ? LOW  : HIGH); }
void Led::off() { g_mcp.write1(_pin, _activeLow ? HIGH : LOW ); }
void Led::toggle() {
  int level = g_mcp.read1(_pin);  // read current pin state (0/1) :contentReference[oaicite:2]{index=2}

  // ตีความว่า "ตอนนี้ติดอยู่ไหม" โดยคำนึงถึง activeLow
  bool isOn = _activeLow ? (level == LOW) : (level == HIGH);

  if (isOn) off();
  else      on();
}


/* =========================================================
 *  Relay
 * ========================================================= */
Relay relay;

static const uint8_t RELAY_PINS[RELAY_COUNT] = {
  RELAY_0_PIN, RELAY_1_PIN, RELAY_2_PIN, RELAY_3_PIN
};

Relay::Relay() {
  for (uint8_t i = 0; i < RELAY_COUNT; i++) _status[i] = false;
}

void Relay::begin() {
  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    pinMode(RELAY_PINS[i], OUTPUT);
    digitalWrite(RELAY_PINS[i], LOW);  // Active HIGH relay => OFF
    _status[i] = false;
  }
}
void Relay::on(uint8_t ch) {
  if (ch >= RELAY_COUNT) return;
  digitalWrite(RELAY_PINS[ch], HIGH);
  _status[ch] = true;
}
void Relay::off(uint8_t ch) {
  if (ch >= RELAY_COUNT) return;
  digitalWrite(RELAY_PINS[ch], LOW);
  _status[ch] = false;
}
void Relay::toggle(uint8_t ch) {
  if (ch >= RELAY_COUNT) return;
  _status[ch] ? off(ch) : on(ch);
}
bool Relay::get(uint8_t ch) const {
  return (ch < RELAY_COUNT) ? _status[ch] : false;
}

/* =========================================================
 *  Switch
 * ========================================================= */
Switch sw[SW_COUNT] = {Switch(SW_0_PIN), Switch(SW_1_PIN), Switch(SW_2_PIN), Switch(SW_3_PIN)};

Switch::Switch(uint8_t swPin) : _sw(swPin) {}
void Switch::begin() { pinMode(_sw, INPUT); }
bool Switch::isPressed() { return (digitalRead(_sw) == LOW); }

/* =========================================================
 *  ADC (MCP342x@1.0.4)
 * ========================================================= */
HS_ADC adc;

HS_ADC::HS_ADC()
  : _addr(HS_MCP3424_ADDR),
    _adc(_addr),
    _ok(false),
    _vref(2.048f),
    _r1(100.0f),
    _r2(10.0f),
    _mode(MCP342x::oneShot),
    _res(MCP342x::resolution14),
    _gain(MCP342x::gain1) {}

bool HS_ADC::begin(uint32_t i2cClockHz, bool doReset, uint16_t resetDelayMs) {
  hsI2CBeginOnce(i2cClockHz);

  if (doReset) {
    MCP342x::generalCallReset();
    delay(resetDelayMs);
  }

  Wire.requestFrom((int)_addr, 1);
  if (!Wire.available()) {
    _ok = false;
    return false;
  }
  _ok = true;
  return true;
}

MCP342x::Channel HS_ADC::toChannelEnum(uint8_t ch) {
  switch (ch) {
    case 0: return MCP342x::channel1;
    case 1: return MCP342x::channel2;
    case 2: return MCP342x::channel3;
    case 3: return MCP342x::channel4;
    default: return MCP342x::channel1;
  }
}

long HS_ADC::fullScaleCounts() const {
  const int r = (int)_res;
  if (r == (int)MCP342x::resolution12) return 2048L;
  if (r == (int)MCP342x::resolution14) return 8192L;
  if (r == (int)MCP342x::resolution16) return 32768L;
  if (r == (int)MCP342x::resolution18) return 131072L;
  return 8192L;
}

bool HS_ADC::readRaw(uint8_t ch, long &raw, MCP342x::Config *outStatus, unsigned long timeout) {
  if (!_ok) return false;
  if (ch >= ADC_CHANNELS) return false;

  MCP342x::Config status;
  raw = 0;

  uint8_t err = _adc.convertAndRead(
    toChannelEnum(ch),
    _mode,
    _res,
    _gain,
    timeout,
    raw,
    status
  );

  if (outStatus) *outStatus = status;
  return (err == 0);
}

bool HS_ADC::readVoltage(uint8_t ch, float &v_adc, unsigned long timeout) {
  long raw = 0;
  if (!readRaw(ch, raw, nullptr, timeout)) return false;

  const float fs = (float)fullScaleCounts();
  v_adc = ((float)raw * _vref) / fs;
  return true;
}

bool HS_ADC::readBatteryVoltage(uint8_t ch, float &v_batt, unsigned long timeout) {
  float v_adc = 0;
  if (!readVoltage(ch, v_adc, timeout)) return false;
  if (_r2 <= 0.00001f) return false;
  v_batt = v_adc * ((_r1 + _r2) / _r2);
  return true;
}

/* =========================================================
 *  HS_Modbus (MAX13487 auto-direction friendly)
 * ========================================================= */
HS_Modbus::HS_Modbus(HardwareSerial &uart, int rxPin, int txPin, int dePin)
  : _uart(uart), _mb(), _rxPin(rxPin), _txPin(txPin), _dePin(dePin), _baud(9600) {}

void HS_Modbus::begin(uint32_t baud, SerialConfig serialConfig) {
  _baud = (int)baud;

#if defined(ESP32)
  _uart.begin(_baud, serialConfig, _rxPin, _txPin);
#else
  _uart.begin(_baud);
#endif

  // ถ้าไม่ได้ต่อ DE/RE => ไม่ต้องแตะอะไร (MAX13487 auto-direction)
  if (_dePin >= 0) {
    pinMode(_dePin, OUTPUT);
    digitalWrite(_dePin, HIGH);
  }

  _mb.begin(1, _uart);
}

bool HS_Modbus::readRegisters(uint8_t deviceId,
                              uint16_t regAddr,
                              uint16_t regCount,
                              uint16_t *outRegs,
                              uint8_t maxOut) {
  if (!outRegs) return false;
  if (regCount == 0 || regCount > maxOut) return false;

  _mb.begin(deviceId, _uart);
  uint8_t result = _mb.readHoldingRegisters(regAddr, regCount);
  if (result != _mb.ku8MBSuccess) return false;

  for (uint8_t i = 0; i < regCount; i++) outRegs[i] = _mb.getResponseBuffer(i);
  return true;
}

/* =========================================================
 *  ModbusSensor
 * ========================================================= */
ModbusSensor::ModbusSensor(HS_Modbus *modbus,
                           uint8_t deviceId,
                           uint16_t regAddr,
                           uint16_t regCount,
                           DataType dtype)
  : _modbus(modbus),
    _deviceId(deviceId),
    _regAddr(regAddr),
    _regCount(regCount),
    _dtype(dtype),
    _lastValue(NAN),
    _lastTs(0) {}

void ModbusSensor::begin() {}

bool ModbusSensor::read() {
  if (!_modbus) return false;
  if (_regCount > 8) return false;

  uint16_t regs[8] = {0};
  if (!_modbus->readRegisters(_deviceId, _regAddr, _regCount, regs, 8)) {
    _lastValue = NAN;
    return false;
  }

  _lastValue = decode(regs, (uint8_t)_regCount);
  _lastTs = millis();
  return true;
}

float ModbusSensor::get() const { return _lastValue; }
uint32_t ModbusSensor::lastTimestamp() const { return _lastTs; }

float ModbusSensor::decode(uint16_t *regs, uint8_t count) {
  if (!regs) return NAN;

  if ((_dtype == DT_UINT16 || _dtype == DT_INT16) && count >= 1) {
    return (_dtype == DT_UINT16) ? (float)regs[0] : (float)(int16_t)regs[0];
  }

  if ((_dtype == DT_UINT32 || _dtype == DT_INT32 || _dtype == DT_FLOAT32) && count >= 2) {
    uint32_t u32 = ((uint32_t)regs[0] << 16) | regs[1];

    if (_dtype == DT_UINT32) return (float)u32;
    if (_dtype == DT_INT32)  return (float)(int32_t)u32;

    union { uint32_t u; float f; } conv;
    conv.u = u32;
    return conv.f;
  }

  return NAN;
}

/* =========================================================
 *  RTC (DS1307 via RTClib)
 * ========================================================= */
static RTC_DS1307 _rtc;
HS_RTC rtc;

bool HS_RTC::begin() {
  hsI2CBeginOnce(HS_I2C_CLOCK_HZ);
  if (!_rtc.begin()) return false;

  if (!_rtc.isrunning()) {
    _rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  return true;
}

bool HS_RTC::isRunning() {
  return _rtc.isrunning();
}

time_t HS_RTC::now() {
  DateTime dt = _rtc.now();
  return (time_t)dt.unixtime();
}

void HS_RTC::set(time_t t) {
  _rtc.adjust(DateTime((uint32_t)t));
}

void HS_RTC::getDateTime(struct tm &out) {
  DateTime dt = _rtc.now();
  out.tm_year = dt.year() - 1900;
  out.tm_mon  = dt.month() - 1;
  out.tm_mday = dt.day();
  out.tm_hour = dt.hour();
  out.tm_min  = dt.minute();
  out.tm_sec  = dt.second();
  out.tm_wday = dt.dayOfTheWeek();  // 0=Sunday
}
