#include "TimeSync.h"
#include <time.h>
#include <sys/time.h>
#include "AppConfig.h"
#include "HS_Elecsensor.h"   // ✅ มี rtc (HS_RTC rtc)

static TimeSyncConfig g_cfg;
static bool g_inited = false;

static uint32_t g_lastNtpRequestMs = 0;
static uint32_t g_lastRtcWriteMs   = 0;
static bool g_ntpConfigured = false;

static bool isEpochSane(time_t t){
  return t > 1700000000; // ~ปลายปี 2023 กันเวลามั่ว
}

bool timeIsReady(){
  return isEpochSane(time(nullptr));
}

void timeDebugPrint(const char* tag){
  time_t now = time(nullptr);
  Serial.printf("[TIME] %s epoch=%ld ready=%s\n", tag, (long)now, timeIsReady()?"YES":"NO");
  if(timeIsReady()){
    struct tm tmLocal;
    localtime_r(&now, &tmLocal);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tmLocal);
    Serial.printf("[TIME] %s local=%s\n", tag, buf);
  }
}

static bool setSystemTime(time_t t){
  if(!isEpochSane(t)) return false;
  timeval tv;
  tv.tv_sec  = t;
  tv.tv_usec = 0;
  settimeofday(&tv, nullptr);
  return true;
}

static bool setSystemFromRtc(){
  // rtc.begin() ต้องเคยถูกเรียกอย่างน้อย 1 ครั้ง
  time_t t = rtc.now();
  if(!isEpochSane(t)) return false;
  bool ok = setSystemTime(t);
  if(ok) Serial.printf("[TIME] system set from RTC epoch=%ld\n", (long)t);
  return ok;
}

static void maybeUpdateRtcFromSystem(){
  if(!timeIsReady()) return;

  // จำกัดการเขียน RTC ถี่เกินไป
  if(millis() - g_lastRtcWriteMs < 60000UL) return;

  time_t sys = time(nullptr);
  time_t rt  = rtc.now();

  if(!isEpochSane(rt)){
    // RTC ไม่พร้อม/ยังมั่ว -> เขียนทับเลย
    rtc.set(sys);
    g_lastRtcWriteMs = millis();
    Serial.printf("[TIME] RTC was invalid -> set RTC = %ld\n", (long)sys);
    return;
  }

  long diff = labs((long)(sys - rt));
  if((uint32_t)diff >= g_cfg.rtcUpdateThresholdSec){
    rtc.set(sys);
    g_lastRtcWriteMs = millis();
    Serial.printf("[TIME] RTC updated (diff=%ld sec)\n", diff);
  }
}

bool timeSyncBegin(const TimeSyncConfig& cfg){
  g_cfg = cfg;

  // เริ่ม RTC
  bool rtcOk = rtc.begin();
  Serial.printf("[TIME] rtc.begin: %s\n", rtcOk ? "OK" : "FAIL");

  // ตั้ง TZ ให้ localtime_r เป็นไทย (UTC+7)
  //setenv("TZ", "WIB-7", 1);
  //tzset();

  // ถ้ายังไม่มีเวลา system -> ใช้ RTC ยัดเข้า system ก่อน (offline-first)
  if(!timeIsReady()){
    if(rtcOk){
      bool ok = setSystemFromRtc();
      Serial.printf("[TIME] init system from RTC: %s\n", ok?"OK":"FAIL");
    } else {
      Serial.println("[TIME] no RTC, system time not ready yet");
    }
  }

  g_inited = true;
  return true;
}

static void requestNtpOnce(){
  // Bangkok = UTC+7 ไม่มี DST
  Serial.printf("[TIME] requestNtpOnce() wifi ok, before epoch=%ld\n", (long)time(nullptr));
  configTime(7 * 3600, 0, g_cfg.ntp1, g_cfg.ntp2);
  g_ntpConfigured = true;
  g_lastNtpRequestMs = millis();
  Serial.println("[TIME] NTP requested (UTC+7)");
}

void timeSyncLoop(bool wifiConnected){
  if(!g_inited) return;

  static uint32_t lastPrint=0;
  if(millis()-lastPrint>2000){
    lastPrint=millis();
    Serial.printf("[TIME] loop wifi=%d ntpCfg=%d epoch=%ld\n",
      wifiConnected?1:0, g_ntpConfigured?1:0, (long)time(nullptr));
  }

  // ถ้า offline และเวลา system ยังไม่พร้อม -> พยายามตั้งจาก RTC ซ้ำเป็นครั้งคราว
  if(!wifiConnected){
    static uint32_t lastTryRtcMs = 0;
    if(!timeIsReady() && millis() - lastTryRtcMs > 3000){
      lastTryRtcMs = millis();
      setSystemFromRtc();
    }
    return;
  }

  // online: ขอ NTP ถ้ายังไม่เคยขอ หรือครบกำหนด resync
  if(wifiConnected){
    if(!g_ntpConfigured){
      requestNtpOnce();
    } else {
      uint32_t ageSec = (millis() - g_lastNtpRequestMs) / 1000UL;
      if(ageSec >= g_cfg.ntpResyncEverySec){
        requestNtpOnce();
      }
    }
  }

  // ถ้า time พร้อมแล้ว ค่อยอัปเดต RTC เป็นครั้งคราว (ลดการเขียน)
  if(timeIsReady()){
    maybeUpdateRtcFromSystem();
  }
}
