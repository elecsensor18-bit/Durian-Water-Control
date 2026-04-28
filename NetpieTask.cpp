#include "NetpieConfig.h"
#include "NetpieTask.h"

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#include "AppConfig.h"
#include "AppState.h"
#include "CmdStruct.h"

#include "TimeSync.h"
#include "WifiManager.h"   // ✅ ใช้ wifiIsConnected()
#include "storage.h"       // ✅ SD backlog + pending report

static NetpieCreds g_netpieCreds;
static WiFiClient wifiClient;
static PubSubClient mqtt(wifiClient);

static uint32_t lastShadowSensor = 0;
static uint32_t lastMsgSensor = 0;
static uint32_t lastSyncSD = 0;

int flag_st;

static int hhmmToMinute(const char* s){
  if(!s) return -1;
  int h=0,m=0;
  if(sscanf(s,"%d:%d",&h,&m)!=2) return -1;
  if(h<0||h>23||m<0||m>59) return -1;
  return h*60+m;
}

static bool mqttOnline(){
  return mqtt.connected();
}

/* -------------------- CMD parse -------------------- */
static bool parseDesiredCmdToStruct(const byte* payload, unsigned int length, CmdStruct& out){
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload, length);
  if (err) return false;

  // payload ตัวอย่างคุณอยู่ที่ root.data.desired.cmd
  JsonObject root = doc.as<JsonObject>();
  JsonObject data = root["data"];
  JsonObject desired = data["desired"];
  JsonObject cmd = desired["cmd"];
  if (cmd.isNull()) return false;

  out.id = String((const char*)(cmd["id"] | ""));
  out.ts = cmd["ts"] | 0;

  const char* mode = cmd["mode"] | "MANUAL";
  if (strcmp(mode, "MANUAL") == 0) out.mode = ControlMode::MANUAL;
  else if (strcmp(mode, "MINMAX") == 0) out.mode = ControlMode::MINMAX;
  else if (strcmp(mode, "TIMER") == 0) out.mode = ControlMode::TIMER;
  else out.mode = ControlMode::TIMER_MINMAX;

  out.targetsMask = (uint8_t)(cmd["targets"]["mask"] | 0x0F);
  out.manualMask  = (uint8_t)(cmd["manual"]["mask"]  | 0);

  out.minVal = cmd["minmax"]["min"] | out.minVal;
  out.maxVal = cmd["minmax"]["max"] | out.maxVal;
  out.hyst   = cmd["minmax"]["hyst"]| out.hyst;

  const char* fs = cmd["safety"]["failSafe"] | "OFF";
  out.failSafeOff = (strcmp(fs, "OFF") == 0);
  out.sensorTimeoutSec = (uint16_t)(cmd["safety"]["sensorTimeoutSec"] | 300);

  // windows รองรับทั้ง object {"0":{...}} และ array [{...}]
  out.windowCount = 0;
  JsonVariant winsV = cmd["timer"]["windows"];

  if(!winsV.isNull()){
    if(winsV.is<JsonObject>()){
      JsonObject wins = winsV.as<JsonObject>();
      for(JsonPair kv : wins){
        if(out.windowCount >= CmdStruct::MAX_WINDOWS) break;
        JsonObject w = kv.value().as<JsonObject>();

        TimerWindow& tw = out.windows[out.windowCount];
        tw.en       = w["en"] | false;
        tw.daysMask  = (uint8_t)(w["daysMask"] | 0);

        const char* startStr = w["start"] | "";
        int st = hhmmToMinute(startStr);
        flag_st =st;
        tw.startMin   = (uint16_t)((st >= 0) ? st : 65535);

        tw.durationMin = (uint16_t)(w["durationMin"] | 0);
        tw.relayMask   = (uint8_t)(w["mask"] | out.targetsMask);

        out.windowCount++;
      }
    } else if(winsV.is<JsonArray>()){
      JsonArray wins = winsV.as<JsonArray>();
      for(JsonVariant v : wins){
        if(out.windowCount >= CmdStruct::MAX_WINDOWS) break;
        JsonObject w = v.as<JsonObject>();

        TimerWindow& tw = out.windows[out.windowCount];
        tw.en       = w["en"] | false;
        tw.daysMask  = (uint8_t)(w["daysMask"] | 0);

        const char* startStr = w["start"] | "";
        int st = hhmmToMinute(startStr);
        tw.startMin   = (uint16_t)((st >= 0) ? st : 65535);

        tw.durationMin = (uint16_t)(w["durationMin"] | 0);
        tw.relayMask   = (uint8_t)(w["mask"] | out.targetsMask);

        out.windowCount++;
      }
    }
  }

  return true;
}

/* -------------------- MQTT callback -------------------- */
static void onMqttMessage(char* topic, byte* payload, unsigned int length){
  Serial.print("\n[MQTT] topic="); Serial.print(topic);
  Serial.print(" len="); Serial.println(length);

  Serial.print("[MQTT] payload=");
  for(unsigned int i=0;i<length;i++) Serial.print((char)payload[i]);
  Serial.println();

  CmdStruct cmd;
  if(!parseDesiredCmdToStruct(payload, length, cmd)){
    Serial.println("[MQTT] no desired.cmd (ignored)");
    return;
  }

  Serial.printf("✅ [CMD] id=%s mode=%u targets=%u manual=%u\n",
    cmd.id.c_str(), (unsigned)cmd.mode, (unsigned)cmd.targetsMask, (unsigned)cmd.manualMask
  );

  // รับเฉพาะ cmd ใหม่จริง
  String applied = STATE.getAppliedCmdId();
  if(cmd.id.length()==0 || cmd.id == applied){
    Serial.println("[CMD] same id -> ignore/no-report");
    return;
  }

  STATE.setPendingCmd(cmd);
  STATE.setCmdDirty(true); // ControlTask จะเป็นคน apply + ตั้ง ackPending
}

/* -------------------- MQTT connect -------------------- */
static void ensureMqtt(){
  if(mqtt.connected()) return;
  if(!wifiIsConnected()) return;

  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(onMqttMessage);
  mqtt.setBufferSize(2048);

  Serial.print("[MQTT] Connecting... ");
  //bool ok = mqtt.connect(NETPIE_CLIENT_ID, NETPIE_TOKEN, NETPIE_SECRET);
  bool ok = mqtt.connect(g_netpieCreds.clientId.c_str(), g_netpieCreds.token.c_str(), g_netpieCreds.secret.c_str());
  Serial.println(ok ? "OK" : "FAIL");
  if(!ok) return;

  mqtt.subscribe(TOPIC_SHADOW_UPDATED);
  mqtt.subscribe(TOPIC_SHADOW_GET_RESPONSE);

  // ขอ shadow ปัจจุบัน
  mqtt.publish(TOPIC_SHADOW_GET, "{}");
}

/* -------------------- Publish with offline storage -------------------- */
static bool publishShadowSensor10min(){
  SensorData s = STATE.getSensor();

  JsonDocument doc;
  JsonObject data = doc["data"].to<JsonObject>();
  JsonObject rep  = data["reported"].to<JsonObject>();
  JsonObject rs   = rep["sensor"].to<JsonObject>();

  rs["soil"] = s.soil;
  rs["temp"] = s.temp;
  //rs["ec"]   = s.ec;
  rs["airTemp"] = s.airTemp;
  rs["airHum"] = s.airHum;
  rs["tsMs"] = s.tsMs;

  char buf[320];
  size_t n = serializeJson(doc, buf, sizeof(buf));

  bool ok = false;
  if(mqttOnline()){
    ok = mqtt.publish(TOPIC_SHADOW_UPDATE, buf, n);
  }

  Serial.printf("[PUB] shadow/sensor10m: %s (len=%u)\n", ok?"OK":"FAIL", (unsigned)n);

  // offline => save SD backlog
  if(!ok){
    time_t now = time(nullptr);
    bool saved = Storage::saveEventSD(CLOUD_NETPIE, TOPIC_SHADOW_UPDATE, buf, now);
    Serial.printf("[SD] shadow backlog: %s\n", saved?"OK":"FAIL");
  }

  return ok;
}

static bool publishReportedAckControlOnce(){
  AckInfo ack = STATE.getAck();
  ControlRuntime rt = STATE.getRuntime();

  JsonDocument doc;
  JsonObject data = doc["data"].to<JsonObject>();
  JsonObject rep  = data["reported"].to<JsonObject>();

  JsonObject ra = rep["ack"].to<JsonObject>();
  ra["lastCmdId"]   = ack.lastCmdId;
  ra["appliedAtMs"] = ack.appliedAtMs;
  ra["result"]      = ack.result;
  ra["reason"]      = ack.reason;

  JsonObject rc = rep["control"].to<JsonObject>();
  rc["activeMode"] = rt.activeMode;
  rc["relayMask"]  = rt.relayMask;
  rc["inWindow"]   = rt.inWindow;
  rc["decision"]   = rt.decision;
  rc["minStart"]   = flag_st;
  rc["why"]        = rt.why;

  char buf[420];
  size_t n = serializeJson(doc, buf, sizeof(buf));

  bool ok = false;
  if(mqttOnline()){
    ok = mqtt.publish(TOPIC_SHADOW_UPDATE, buf, n);
  }

  Serial.printf("[PUB] reported/ack+control: %s (len=%u)\n", ok?"OK":"FAIL", (unsigned)n);

  // offline => pending report (send once later)
  if(!ok){
    bool saved = Storage::savePendingReport(TOPIC_SHADOW_UPDATE, buf);
    Serial.printf("[PREF] pending report saved: %s\n", saved?"OK":"FAIL");
  }

  return ok;
}

static bool publishReportedControlOnly(){
  ControlRuntime rt = STATE.getRuntime();

  JsonDocument doc;
  JsonObject data = doc["data"].to<JsonObject>();
  JsonObject rep  = data["reported"].to<JsonObject>();
  JsonObject rc   = rep["control"].to<JsonObject>();

  rc["activeMode"] = rt.activeMode;
  rc["relayMask"]  = rt.relayMask;
  rc["inWindow"]   = rt.inWindow;
  rc["decision"]   = rt.decision;
  rc["why"]        = rt.why;

  char buf[320];
  size_t n = serializeJson(doc, buf, sizeof(buf));

  bool ok = false;
  if(mqttOnline()){
    ok = mqtt.publish(TOPIC_SHADOW_UPDATE, buf, n);
  }
  Serial.printf("[PUB] reported/control: %s (len=%u)\n", ok?"OK":"FAIL", (unsigned)n);

  // ถ้า fail และคุณอยากให้ “control report” ไม่หาย: จะ save pending report ก็ได้
  if(!ok){
    Storage::savePendingReport(TOPIC_SHADOW_UPDATE, buf);
  }
  return ok;
}

static bool publishMsgSensorOptional(){
#if (MSG_SENSOR_INTERVAL_MS > 0)
  SensorData s = STATE.getSensor();
  JsonDocument doc;
  doc["soil"]=s.soil; doc["temp"]=s.temp; //doc["ec"]=s.ec;

  char buf[160];
  size_t n = serializeJson(doc, buf, sizeof(buf));

  bool ok = false;
  if(mqttOnline()){
    ok = mqtt.publish(TOPIC_MSG_SENSOR, buf, n);
  }

  Serial.printf("[PUB] @msg/sensor: %s (len=%u)\n", ok?"OK":"FAIL", (unsigned)n);
  return ok;
#else
  return true;
#endif
}

/* -------------------- Storage sync hooks -------------------- */
static bool pubCb(CloudType cloud, const char* topic, const char* payload){
  (void)cloud;
  if(!mqttOnline()) return false;
  return mqtt.publish(topic, payload);
}

static void trySendPendingReport(){
  if(!mqttOnline()) return;
  if(!Storage::hasPendingReport()) return;

  String t, p;
  if(!Storage::loadPendingReport(t, p)) return;

  bool ok = mqtt.publish(t.c_str(), p.c_str());
  Serial.printf("[PREF] send pending report: %s\n", ok?"OK":"FAIL");
  if(ok) Storage::clearPendingReport();
}

/* -------------------- Task -------------------- */
static void netpieTask(void*){
  // init storage once
  StorageConfig sc;
  sc.sdCsPin = SD_CS_PIN;
  sc.spiFreqHz = 1000000;
  sc.maxSyncFiles = 2;
  sc.maxSyncLines = 20;
  Storage::begin(sc);

  loadNetpieCreds(g_netpieCreds);

  Serial.println("[NETPIE] Loaded credentials");
  Serial.println("[NETPIE] clientId=" + g_netpieCreds.clientId);

  while(true){
  bool online = wifiIsConnected();

  // ✅ ให้ time sync วิ่งตลอด
  timeSyncLoop(online);

  if(!online){
    vTaskDelay(pdMS_TO_TICKS(200));
    continue;
  }
    ensureMqtt();

    if(mqtt.connected()){
      mqtt.loop();

      uint32_t nowMs = millis();

      // 1) Shadow sensor ทุก 10 นาที
      if(nowMs - lastShadowSensor >= SHADOW_SENSOR_10MIN_MS){
        lastShadowSensor = nowMs;
        publishShadowSensor10min();
      }

      // 2) optional msg sensor
#if (MSG_SENSOR_INTERVAL_MS > 0)
      if(nowMs - lastMsgSensor >= MSG_SENSOR_INTERVAL_MS){
        lastMsgSensor = nowMs;
        publishMsgSensorOptional();
      }
#endif

      // 3) report ack/control เฉพาะเมื่อ ControlTask apply cmd ใหม่
      if(STATE.takeAckPending()){
        publishReportedAckControlOnce(); // ถ้าส่ง fail จะ pending report เอง
      }

      // report control เมื่อ runtime เปลี่ยน
      if(STATE.takeControlPending()){
        publishReportedControlOnly();
      }
      
      // 4) ส่ง pending report ก่อน
      trySendPendingReport();

      // 5) sync SD backlog เป็นระยะ (30 วินาที)
      if(nowMs - lastSyncSD >= 30000){
        lastSyncSD = nowMs;
        uint32_t sent = Storage::syncSD(CLOUD_NETPIE, pubCb);
        if(sent){
          Serial.printf("[SYNC] sent %u lines\n", (unsigned)sent);
        }
      }
    }

    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

void startNetpieTask(uint32_t stackWords, UBaseType_t prio){
  xTaskCreatePinnedToCore(netpieTask, "NetpieTask", stackWords, nullptr, prio, nullptr, 0);
}
