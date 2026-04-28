// storage.h  (ESP32/Arduino)  ✅ Ready-to-use 100%
// - SD backlog: save JSON-lines per cloud (/netpie, /firebase)
// - Sync when online: read line-by-line, publish via callback(topic,payload)
// - Optional: store ONE pending "report event" in Preferences (flash) to avoid SD duplicates
#pragma once
#include <Arduino.h>

#include <SPI.h>
#include <SD.h>

#if defined(ESP32)
  #include <Preferences.h>
#endif

enum CloudType : uint8_t {
  CLOUD_NETPIE   = 0,
  CLOUD_FIREBASE = 1
};

struct StorageConfig {
  int      sdCsPin      = 5;
  uint32_t spiFreqHz    = 1000000;
  uint8_t  maxSyncFiles = 2;    // per sync() call
  uint16_t maxSyncLines = 20;   // per file per sync() call
};

using PublishFn = bool (*)(CloudType cloud, const char* topic, const char* payload);

class Storage {
public:
  // ---------- lifecycle ----------
  static bool begin(const StorageConfig& cfg = StorageConfig());
  static bool sdReady();

  // ---------- SD backlog ----------
  // Save a single "event line" (JSON string) to SD in cloud dir
  static bool saveLineSD(CloudType cloud, const char* line, time_t now);

  // Save as structured event line:
  // {"ts":..., "topic":"...", "payload":"{...json...}"}
  // NOTE: payload is kept as a JSON string (no re-parse needed during save)
  static bool saveEventSD(CloudType cloud, const char* topic, const char* payloadJson, time_t now);

  // Sync backlog from SD:
  // - publishes up to cfg.maxSyncFiles files and cfg.maxSyncLines lines per file
  // - returns number of successfully published lines this call
  static uint32_t syncSD(CloudType cloud, PublishFn publisher);

  // ---------- Pending report (Preferences) ----------
  // Use this for "report event" that must not duplicate and should be sent once.
  // Store one pending item: (topic,payloadJson)
  static bool savePendingReport(const char* topic, const char* payloadJson);
  static bool hasPendingReport();
  static bool loadPendingReport(String& outTopic, String& outPayloadJson);
  static void clearPendingReport();

private:
  static StorageConfig _cfg;

  // SD init / concurrency guard
  static bool _sdInited;
  static bool _sdOK;

#if defined(ESP32)
  static Preferences _prefs;
#endif

  static String cloudDir(CloudType cloud);
  static String dailyFilePath(CloudType cloud, time_t now);

  static void lockSD();
  static void unlockSD();
  static void lockPrefs();
  static void unlockPrefs();

  // simple mutex (FreeRTOS if ESP32, fallback no-op otherwise)
  static void* _mtxSD;
  static void* _mtxPrefs;
};
