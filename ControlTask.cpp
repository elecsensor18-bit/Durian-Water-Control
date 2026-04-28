#include "ControlTask.h"
#include "AppState.h"
#include "CmdStore.h"
#include "HS_Elecsensor.h"
#include <time.h>

static uint8_t m4(uint8_t v){ return v & 0x0F; }

static bool sensorStale(const CmdStruct& cmd, const SensorData& s){
  uint32_t age = millis() - s.tsMs;
  return age > (uint32_t)cmd.sensorTimeoutSec * 1000UL;
}

static void applyRelayMask(uint8_t mask){
  mask &= 0x0F;
  for(int i=0;i<RELAY_COUNT;i++){
    if(mask & (1<<i)) relay.on(i);
    else              relay.off(i);
  }
}

// readback “ตามที่สั่ง” จากไลบรารี่
static uint8_t relayReadback(){
  uint8_t actual = 0;
  for(int i=0;i<RELAY_COUNT;i++){
    if(relay.get(i)) actual |= (1<<i);
  }
  return actual & 0x0F;
}

static bool timeReady(){
  time_t now = time(nullptr);
  return now > 1700000000; // กันค่าเวลาไม่พร้อม (approx)
}

static bool nowWdayMin(int& wday, int& minOfDay){
  if(!timeReady()) return false;
  time_t t = time(nullptr);
  struct tm tmLocal;
  localtime_r(&t, &tmLocal);
  wday = tmLocal.tm_wday; // 0=Sun
  minOfDay = tmLocal.tm_hour*60 + tmLocal.tm_min;
  return true;
}

// รองรับ wrap ข้ามเที่ยงคืน (เผื่อ duration ล้น 24:00)
static bool inWindowMinute(int mod, uint16_t start, uint16_t dur){
  if(start > 1439 || dur == 0) return false;
  uint16_t end = start + dur;
  if(end <= 1440){
    return (mod >= (int)start && mod < (int)end);
  } else {
    // wrap midnight
    uint16_t end2 = (uint16_t)(end - 1440);
    return (mod >= (int)start) || (mod < (int)end2);
  }
}

static bool inTimerWindow(const CmdStruct& cmd, bool& inWin, uint8_t& allowedMask, String& why){
  inWin = false;
  allowedMask = m4(cmd.targetsMask);

  int wday=0, mod=0;
  if(!nowWdayMin(wday, mod)){
    why = "time_not_ready";
    Serial.println("[TIMER] time_not_ready");
    return false;
  }

  Serial.printf("[TIMER] now wday=%d mod=%d (hh=%d mm=%d) winCount=%u targets=%u\n",
    wday, mod, mod/60, mod%60, (unsigned)cmd.windowCount, (unsigned)m4(cmd.targetsMask)
  );

  if(cmd.windowCount == 0){
    why="no_windows";
    Serial.println("[TIMER] no_windows");
    return true;
  }

  for(uint8_t i=0;i<cmd.windowCount;i++){
    const TimerWindow& w = cmd.windows[i];

    bool passEn  = w.en;
    bool passDay = (((w.daysMask >> wday) & 1) != 0);
    bool passTime = inWindowMinute(mod, w.startMin, w.durationMin);

    Serial.printf("[TIMER] #%u en=%d daysMask=%u passDay=%d start=%u dur=%u mod=%d passTime=%d mask=%u\n",
      i, (int)w.en, (unsigned)w.daysMask, (int)passDay,
      (unsigned)w.startMin, (unsigned)w.durationMin, mod, (int)passTime,
      (unsigned)m4(w.relayMask)
    );

    if(!passEn)  continue;
    if(!passDay) continue;

    if(passTime){
      inWin = true;
      allowedMask = (uint8_t)(m4(w.relayMask) & m4(cmd.targetsMask));
      why = "timer_on";
      Serial.printf("[TIMER] ✅ IN WINDOW -> allowed=%u\n", (unsigned)allowedMask);
      return true;
    }
  }

  why="timer_off";
  Serial.println("[TIMER] ❌ timer_off (no window matched)");
  return true;
}

static uint8_t decideMinmax(const CmdStruct& cmd, const SensorData& s, uint8_t allowedMask,
                            const ControlRuntime& prev, String& why){
  float soil = (float)s.soil;   // ถ้า soil คุณเป็น 0-100 หรือ 0-1023 ให้เทียบกับ min/max ให้ถูกหน่วย
  float minV = cmd.minVal;
  float maxV = cmd.maxVal;
  float h    = cmd.hyst;

  if(maxV < minV){ why="minmax_bad_range"; return 0; }

  if(soil < (minV - h)){ why="soil_below_min"; return allowedMask; }
  if(soil > (maxV + h)){ why="soil_above_max"; return 0; }

  why="soil_in_band_hold";
  return (uint8_t)(prev.relayMask & allowedMask);
}

static const char* modeStr(ControlMode m){
  switch(m){
    case ControlMode::MANUAL: return "MANUAL";
    case ControlMode::MINMAX: return "MINMAX";
    case ControlMode::TIMER: return "TIMER";
    default: return "TIMER_MINMAX";
  }
}

static ControlRuntime applyAndUpdateRuntime(const CmdStruct& cur, bool forceReport){
  SensorData s = STATE.getSensor();
  ControlRuntime prev = STATE.getRuntime();

  uint8_t targets = m4(cur.targetsMask);
  uint8_t calcMask = 0;
  bool inWin = false;
  String why;

  switch(cur.mode){
    case ControlMode::MANUAL: {
      // ✅ MANUAL ไม่ต้องพึ่ง sensor
      why="manual";
      calcMask = (uint8_t)(m4(cur.manualMask) & targets);
    } break;

    case ControlMode::TIMER: {
      // ✅ TIMER ไม่ต้องพึ่ง sensor
      uint8_t allowed = targets;
      bool ok = inTimerWindow(cur, inWin, allowed, why);
      if(!ok || !inWin){
        calcMask = 0;
        // why ถูก set แล้ว
      } else {
        // เปิดตาม mask ของ window (ถูก AND ด้วย targets แล้วใน inTimerWindow)
        calcMask = m4(allowed);
        why="timer_on";
      }
    } break;

    case ControlMode::MINMAX: {
      // ✅ MINMAX ต้องพึ่ง sensor
      if(sensorStale(cur, s)){
        why="sensor_timeout";
        calcMask = 0;
      } else {
        calcMask = decideMinmax(cur, s, targets, prev, why);
      }
    } break;

    case ControlMode::TIMER_MINMAX: {
      uint8_t allowed = targets;
      bool ok = inTimerWindow(cur, inWin, allowed, why);
      if(!ok || !inWin){
        calcMask = 0;
        // why timer_off / time_not_ready
      } else {
        if(sensorStale(cur, s)){
          why="timer_sensor_timeout";
          calcMask = 0;
        } else {
          calcMask = decideMinmax(cur, s, m4(allowed), prev, why);
          if(why.startsWith("soil_")) why = "timer_" + why;
        }
      }
    } break;
  }

  // ✅ apply แล้ว readback
  applyRelayMask(calcMask);
  uint8_t actual = relayReadback();

  ControlRuntime rt;
  rt.activeMode = modeStr(cur.mode);
  rt.relayMask  = actual;
  rt.inWindow   = inWin;
  rt.decision   = (why.indexOf("_hold")>=0) ? "HOLD" : (actual ? "ON" : "OFF");
  rt.why        = why;

  ControlRuntime prevRt = STATE.getRuntime();   // <-- เอาค่าเดิมก่อน

  STATE.setRuntime(rt);

  bool changed =
    (rt.relayMask != prevRt.relayMask) ||
    (rt.inWindow  != prevRt.inWindow)  ||
    (rt.why       != prevRt.why);

  if(changed){
    STATE.setControlPending(true);     // <-- แจ้ง NetpieTask ว่าต้อง report control
  }

  if(forceReport){
    Serial.printf("[CTRL] APPLY NOW mode=%s targets=%u calc=%u actual=%u why=%s\n",
      rt.activeMode.c_str(), (unsigned)targets, (unsigned)calcMask, (unsigned)actual, rt.why.c_str());
    Serial.printf("[CTRL] sensor tsMs=%lu age=%lu timeout=%u\n",
      (unsigned long)s.tsMs, (unsigned long)(millis()-s.tsMs), (unsigned)cur.sensorTimeoutSec);
  }

  return rt;
}

static void controlTask(void*){
  relay.begin();
  CmdStore::begin();

  CmdStruct cmd;
  if(CmdStore::load(cmd)){
    STATE.setPendingCmd(cmd);
    STATE.setAppliedCmdId(cmd.id);
    Serial.printf("[CTRL] loaded last cmd id=%s (offline-first)\n", cmd.id.c_str());
    applyAndUpdateRuntime(cmd, true);
  } else {
    Serial.println("[CTRL] no cmd in flash yet (waiting cloud)");
  }

  uint32_t lastEval = 0;

  while(true){
    if(STATE.takeCmdDirty()){
      CmdStruct nc = STATE.getPendingCmd();
      String applied = STATE.getAppliedCmdId();

      if(nc.id.length()>0 && nc.id != applied){
        CmdStore::save(nc);
        STATE.setAppliedCmdId(nc.id);

        ControlRuntime rt = applyAndUpdateRuntime(nc, true);

        AckInfo ack;
        ack.lastCmdId = nc.id;
        ack.appliedAtMs = millis();
        ack.result = "OK";
        ack.reason = "applied";

        STATE.setAck(ack);
        STATE.setAckPending(true);

        Serial.printf("[CTRL] ✅ applied NEW cmd id=%s relay=%u -> will report\n",
          nc.id.c_str(), (unsigned)rt.relayMask);
      } else {
        Serial.println("[CTRL] cmd dirty but SAME id -> ignore/no-report");
      }
    }

    uint32_t now = millis();
    if(now - lastEval >= 1000){
      lastEval = now;
      CmdStruct cur = STATE.getPendingCmd();
      applyAndUpdateRuntime(cur, false);
    }

    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

void startControlTask(uint32_t stackWords, UBaseType_t prio){
  xTaskCreatePinnedToCore(controlTask, "ControlTask", stackWords, nullptr, prio, nullptr, 1);
}