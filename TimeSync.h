#pragma once
#include <Arduino.h>

struct TimeSyncConfig {
  const char* tzInfo = "Asia/Bangkok";
  const char* ntp1   = "pool.ntp.org";
  const char* ntp2   = "time.nist.gov";

  // เขียน RTC ถี่ไปไม่ดี: ให้เขียนเมื่อ sync NTP สำเร็จ และต่างเกินกี่วินาที
  uint32_t rtcUpdateThresholdSec = 60;

  // ขอ NTP ซ้ำทุกกี่ชั่วโมง (ตอนออนไลน์)
  uint32_t ntpResyncEverySec = 12UL * 3600UL; // 12 ชั่วโมง
};

// เรียกครั้งเดียวใน setup
bool timeSyncBegin(const TimeSyncConfig& cfg);

// เรียกเป็นระยะใน loop/task
// wifiConnected = true/false
void timeSyncLoop(bool wifiConnected);

// true เมื่อ time(nullptr) พร้อมใช้แล้ว
bool timeIsReady();

// debug
void timeDebugPrint(const char* tag);
