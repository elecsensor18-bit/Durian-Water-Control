#pragma once
#include <Arduino.h>

/* WiFi default (ถ้ายังอยากมีไว้) */
#define WIFI_SSID     "W&M"
#define WIFI_PASS     "0848165050"

/* MQTT */
#define MQTT_HOST     "mqtt.nexiiot.io"
#define MQTT_PORT     1883

/* NETPIE default credentials */
#define DEFAULT_NETPIE_CLIENT_ID  "c0b7ba40-d338-4fdb-9327-2d7fe2f90e43"
#define DEFAULT_NETPIE_TOKEN      "RuzuaMC4jCAHjcSzcbzq9FAYBL1CW9Vv"
#define DEFAULT_NETPIE_SECRET     "uzHMtLerECUB1Y9cfquvFnbE8S7zcEgw"

/* Topics */
#define TOPIC_SHADOW_GET           "@shadow/data/get"
#define TOPIC_SHADOW_UPDATE        "@shadow/data/update"
#define TOPIC_SHADOW_UPDATED       "@shadow/data/updated"
#define TOPIC_SHADOW_GET_RESPONSE  "@private/shadow/data/get/response"

/* Sensor publish policy */
#define SHADOW_SENSOR_10MIN_MS     (10UL * 60UL * 1000UL)

/* Optional realtime sensor msg */
#define MSG_SENSOR_INTERVAL_MS     60000UL
#define TOPIC_MSG_SENSOR           "@msg/sensor"

/* Storage */
#define SD_CS_PIN 5