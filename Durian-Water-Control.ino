#include <Arduino.h>
#include "AppConfig.h"
#include "AppState.h"
#include "CmdStore.h"
#include "SensorTask.h"
#include "ControlTask.h"
#include "TimeSync.h"
#include "WifiManager.h"
#include "NetpieTask.h"

static void onWiFiConnected(){
#if USE_NTP_WHEN_ONLINE
  Serial.println("[WIFI] callback: connected");
#endif
}

void setup(){
  Serial.begin(115200);
  delay(200);

  STATE.begin();
  CmdStore::begin();

  wifiBegin(onWiFiConnected);
  startWiFiTask(4096, 2);

  TimeSyncConfig tc;
  tc.tzInfo = "WIB-7";              // ✅ ใช้จริง
  tc.ntp1 = "pool.ntp.org";
  tc.ntp2 = "time.nist.gov";
  tc.ntpResyncEverySec = 3600;
  tc.rtcUpdateThresholdSec = 5;
  timeSyncBegin(tc);

  // เริ่มด้วย random แน่นอน
  startSensorTask(false);

  startControlTask();
  startNetpieTask();

  Serial.println("=== CMD-only version started ===");
}

void loop(){ delay(1000); }
