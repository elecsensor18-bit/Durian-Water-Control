#pragma once
#include <Arduino.h>

/* ===================== I2C ===================== */
#ifndef HS_I2C_SDA
#define HS_I2C_SDA 21
#endif

#ifndef HS_I2C_SCL
#define HS_I2C_SCL 22
#endif

#ifndef HS_I2C_CLOCK_HZ
#define HS_I2C_CLOCK_HZ 100000UL
#endif

/* ===================== MCP23008 ===================== */
#ifndef HS_MCP23008_ADDR
#define HS_MCP23008_ADDR 0x24
#endif

/* ===================== Buzzer ===================== */
#ifndef BEEP_PIN
#define BEEP_PIN 27
#endif

/* ===================== Relay ===================== */
#ifndef RELAY_COUNT
#define RELAY_COUNT 4
#endif

#ifndef RELAY_0_PIN
#define RELAY_0_PIN 32
#endif
#ifndef RELAY_1_PIN
#define RELAY_1_PIN 33
#endif
#ifndef RELAY_2_PIN
#define RELAY_2_PIN 25
#endif
#ifndef RELAY_3_PIN
#define RELAY_3_PIN 26
#endif

/* ===================== Switch ===================== */
#ifndef SW_COUNT
#define SW_COUNT 4
#endif

#ifndef SW_0_PIN
#define SW_0_PIN 36
#endif
#ifndef SW_1_PIN
#define SW_1_PIN 39
#endif
#ifndef SW_2_PIN
#define SW_2_PIN 34
#endif
#ifndef SW_3_PIN
#define SW_3_PIN 35
#endif

/* ===================== ADC MCP342x ===================== */
#ifndef HS_MCP3424_ADDR
#define HS_MCP3424_ADDR 0x6E   // << คุณใช้ address นี้อยู่เดิม
#endif

#ifndef ADC_CHANNELS
#define ADC_CHANNELS 4
#endif
