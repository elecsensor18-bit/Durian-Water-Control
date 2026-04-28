// storage.cpp  (ESP32/Arduino)  ✅ Ready-to-use 100%
#include "storage.h"
#include <time.h>

// ------------ static fields ------------
StorageConfig Storage::_cfg;
bool Storage::_sdInited = false;
bool Storage::_sdOK = false;

void* Storage::_mtxSD = nullptr;
void* Storage::_mtxPrefs = nullptr;

#if defined(ESP32)
Preferences Storage::_prefs;
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
static inline SemaphoreHandle_t asSem(void* p){ return (SemaphoreHandle_t)p; }
#endif

// ------------ tiny helpers ------------
static bool timeReady(time_t now){
  return now > 1000000000; // ~2001-09-09
}

static String isoDayFileName(time_t now){
  struct tm t;
  localtime_r(&now, &t);
  char buf[32];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02d.log",
           t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
  return String(buf);
}

static bool ensureDirExists(const String& dir){
  if (SD.exists(dir)) return true;
  return SD.mkdir(dir);
}

void Storage::lockSD(){
#if defined(ESP32)
  if (_mtxSD) xSemaphoreTake(asSem(_mtxSD), portMAX_DELAY);
#endif
}
void Storage::unlockSD(){
#if defined(ESP32)
  if (_mtxSD) xSemaphoreGive(asSem(_mtxSD));
#endif
}
void Storage::lockPrefs(){
#if defined(ESP32)
  if (_mtxPrefs) xSemaphoreTake(asSem(_mtxPrefs), portMAX_DELAY);
#endif
}
void Storage::unlockPrefs(){
#if defined(ESP32)
  if (_mtxPrefs) xSemaphoreGive(asSem(_mtxPrefs));
#endif
}

// ------------ public ------------
bool Storage::begin(const StorageConfig& cfg){
  _cfg = cfg;

#if defined(ESP32)
  if (!_mtxSD) _mtxSD = (void*)xSemaphoreCreateMutex();
  if (!_mtxPrefs) _mtxPrefs = (void*)xSemaphoreCreateMutex();

  lockPrefs();
  // open preferences namespace once
  if (!_prefs.begin("storage", false)) {
    unlockPrefs();
    Serial.println("[STO] Preferences begin failed");
    // still allow SD use
  } else {
    unlockPrefs();
  }
#endif

  // SD will be inited lazily by sdReady()
  return true;
}

bool Storage::sdReady(){
  if (_sdInited) return _sdOK;

  lockSD();
  if (_sdInited) { unlockSD(); return _sdOK; }

  _sdInited = true;
  _sdOK = SD.begin(_cfg.sdCsPin, SPI, _cfg.spiFreqHz);

  if (!_sdOK) Serial.println("[SD] init failed");
  else Serial.println("[SD] init OK");

  unlockSD();
  return _sdOK;
}

String Storage::cloudDir(CloudType cloud){
  if (cloud == CLOUD_NETPIE)   return "/netpie";
  if (cloud == CLOUD_FIREBASE) return "/firebase";
  return "/unknown";
}

String Storage::dailyFilePath(CloudType cloud, time_t now){
  String dir = cloudDir(cloud);
  String file = isoDayFileName(now);
  return dir + "/" + file;
}

bool Storage::saveLineSD(CloudType cloud, const char* line, time_t now){
  if (!line || line[0] == '\0') return false;
  if (!timeReady(now)) return false;
  if (!sdReady()) return false;

  lockSD();

  String dir = cloudDir(cloud);
  if (!ensureDirExists(dir)) {
    Serial.println("[SD] mkdir failed");
    unlockSD();
    return false;
  }

  String path = dailyFilePath(cloud, now);
  File f = SD.open(path, FILE_APPEND);
  if (!f) {
    Serial.println("[SD] open failed");
    unlockSD();
    return false;
  }

  size_t n = f.println(line);
  f.flush();
  f.close();

  unlockSD();

  if (n == 0) {
    Serial.println("[SD] write failed");
    return false;
  }

  return true;
}

bool Storage::saveEventSD(CloudType cloud, const char* topic, const char* payloadJson, time_t now){
  if (!topic || topic[0] == '\0') return false;
  if (!payloadJson || payloadJson[0] == '\0') return false;
  if (!timeReady(now)) return false;

  // store as a single-line JSON with payload as string (keeps it simple & robust)
  String line;
  line.reserve(strlen(payloadJson) + strlen(topic) + 64);

  line += "{\"ts\":";
  line += String((uint32_t)now);
  line += ",\"topic\":\"";
  // naive escape for quotes/backslashes in topic (rare). If needed, keep topic simple.
  for (const char* p = topic; *p; ++p) {
    if (*p == '"' || *p == '\\') line += '\\';
    line += *p;
  }
  line += "\",\"payload\":\"";
  // escape payload as JSON string
  for (const char* p = payloadJson; *p; ++p) {
    char c = *p;
    if (c == '"' || c == '\\') line += '\\';
    if (c == '\n') { line += "\\n"; continue; }
    if (c == '\r') { line += "\\r"; continue; }
    line += c;
  }
  line += "\"}";

  return saveLineSD(cloud, line.c_str(), now);
}

static bool parseLineToTopicPayload(const String& line, String& outTopic, String& outPayload){
  // Expected format:
  // {"ts":..., "topic":"...", "payload":"{...json...}"}
  // Minimal parser (no ArduinoJson dependency inside storage)
  int tpos = line.indexOf("\"topic\":\"");
  int ppos = line.indexOf("\"payload\":\"");
  if (tpos < 0 || ppos < 0) return false;

  tpos += 9;
  int tend = line.indexOf('"', tpos);
  if (tend < 0) return false;
  outTopic = line.substring(tpos, tend);

  ppos += 11;
  int pend = line.lastIndexOf('"');
  if (pend <= ppos) return false;

  String esc = line.substring(ppos, pend);

  // unescape \" \\ \n \r
  outPayload = "";
  outPayload.reserve(esc.length());
  for (int i = 0; i < (int)esc.length(); i++){
    char c = esc[i];
    if (c == '\\' && i + 1 < (int)esc.length()){
      char n = esc[i+1];
      if (n == 'n') { outPayload += '\n'; i++; continue; }
      if (n == 'r') { outPayload += '\r'; i++; continue; }
      // \" or \\ or any escaped
      outPayload += n; i++; continue;
    }
    outPayload += c;
  }
  return true;
}

uint32_t Storage::syncSD(CloudType cloud, PublishFn publisher){
  if (!publisher) return 0;
  if (!sdReady()) return 0;

  lockSD();

  String dir = cloudDir(cloud);
  File logDir = SD.open(dir);
  if (!logDir || !logDir.isDirectory()){
    if (logDir) logDir.close();
    unlockSD();
    return 0;
  }

  uint32_t sentCount = 0;
  uint8_t filesDone = 0;

  while (filesDone < _cfg.maxSyncFiles){
    File file = logDir.openNextFile();
    if (!file) break;

    String name = String(file.name());
    file.close();

    String filename = dir + "/" + name;
    if (filename.endsWith(".sent") || filename.endsWith(".sync")){
      continue;
    }

    // mark syncing
    String syncing = filename + ".sync";
    SD.rename(filename, syncing);

    File f = SD.open(syncing, FILE_READ);
    if (!f){
      SD.rename(syncing, filename);
      continue;
    }

    bool allSent = true;
    uint16_t linesThisFile = 0;

    while (f.available()){
      if (linesThisFile >= _cfg.maxSyncLines){
        allSent = false;
        break; // continue next time
      }

      String line = f.readStringUntil('\n');
      line.trim();
      if (line.length() < 10) continue;

      String topic, payload;
      if (!parseLineToTopicPayload(line, topic, payload)){
        // Bad line => skip it (prevent blocking whole file)
        Serial.println("[SD] bad line skipped");
        linesThisFile++;
        continue;
      }

      // publish
      bool ok = publisher(cloud, topic.c_str(), payload.c_str());
      if (!ok){
        allSent = false;
        break;
      }

      sentCount++;
      linesThisFile++;
    }

    f.close();

    if (allSent){
      SD.rename(syncing, filename + ".sent");
    } else {
      // revert so we try again later (simple strategy; may duplicate if partial lines were sent)
      SD.rename(syncing, filename);
    }

    filesDone++;
  }

  logDir.close();
  unlockSD();

  return sentCount;
}

// ---------- pending report (Preferences) ----------
bool Storage::savePendingReport(const char* topic, const char* payloadJson){
#if !defined(ESP32)
  (void)topic; (void)payloadJson;
  return false;
#else
  if (!topic || !topic[0] || !payloadJson || !payloadJson[0]) return false;

  lockPrefs();
  bool ok = true;
  ok &= (_prefs.putString("p_topic", topic) > 0);
  ok &= (_prefs.putString("p_payload", payloadJson) > 0);
  _prefs.putBool("p_has", true);
  unlockPrefs();

  return ok;
#endif
}

bool Storage::hasPendingReport(){
#if !defined(ESP32)
  return false;
#else
  lockPrefs();
  bool has = _prefs.getBool("p_has", false);
  unlockPrefs();
  return has;
#endif
}

bool Storage::loadPendingReport(String& outTopic, String& outPayloadJson){
#if !defined(ESP32)
  (void)outTopic; (void)outPayloadJson;
  return false;
#else
  lockPrefs();
  bool has = _prefs.getBool("p_has", false);
  if (!has){
    unlockPrefs();
    return false;
  }
  outTopic = _prefs.getString("p_topic", "");
  outPayloadJson = _prefs.getString("p_payload", "");
  unlockPrefs();

  return (outTopic.length() > 0 && outPayloadJson.length() > 0);
#endif
}

void Storage::clearPendingReport(){
#if defined(ESP32)
  lockPrefs();
  _prefs.putBool("p_has", false);
  _prefs.remove("p_topic");
  _prefs.remove("p_payload");
  unlockPrefs();
#endif
}
