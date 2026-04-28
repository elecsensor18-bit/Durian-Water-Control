#include "SensorTask.h"
#include "AppState.h"
#include "HS_Elecsensor.h"

#include <Wire.h>
#include <Adafruit_SHT31.h>

// ===== CONFIG =====
#define SENSOR_INTERVAL_MS   5000UL
#define SHT31_ADDR           0x44   // ส่วนใหญ่ 0x44 (บางตัว 0x45)

// ถ้าใช้ random
static bool g_useRandom = false;

// ===== MODBUS (ตามไลบรารี่ HS_Elecsensor) =====
// NOTE: baud ของเซ็นเซอร์ดินหลายรุ่นเป็น 4800 ไม่ใช่ 9600
static HS_Modbus mb(Serial2, 16, 17, -1); // RX=16 TX=17 (ปรับตามบอร์ดคุณ)
static ModbusSensor sMoist(&mb, 1, 0x0000, 1, ModbusSensor::DT_UINT16);
static ModbusSensor sTemp (&mb, 1, 0x0001, 1, ModbusSensor::DT_INT16);
//static ModbusSensor sEC   (&mb, 1, 0x0002, 1, ModbusSensor::DT_UINT16);

// ===== SHT31 =====
static Adafruit_SHT31 sht31;
static bool shtOk = false;

static void i2cEnsureBegin(){
  static bool inited=false;
  if(inited) return;
  inited=true;

  Wire.begin(HS_I2C_SDA, HS_I2C_SCL);
  Wire.setClock(HS_I2C_CLOCK_HZ);
}

static void sht31Begin(){
  i2cEnsureBegin();
  shtOk = sht31.begin(SHT31_ADDR);
  if(shtOk) Serial.println("[SHT31] OK");
  else      Serial.println("[SHT31] FAIL (check wiring/addr 0x44/0x45)");
}

static void readSht31(float &outT, float &outH){
  outT = NAN;
  outH = NAN;
  if(!shtOk) return;

  float t = sht31.readTemperature();
  float h = sht31.readHumidity();
  if(isnan(t) || isnan(h)) return;

  outT = t;
  outH = h;
}

static void sensorTask(void* pv){
  (void)pv;

  Serial.printf("[SENS] task started (random=%d)\n", (int)g_useRandom);

  // init sensors
  if(!g_useRandom){
    // ✅ แนะนำลอง 4800 ก่อน ถ้ารุ่นคุณเป็น 9600 ค่อยเปลี่ยนกลับ
    mb.begin(4800, SERIAL_8N1);
    sMoist.begin();
    sTemp.begin();
    //sEC.begin();
    Serial.println("[SENS] MODBUS init done");

    sht31Begin();
  } else {
    Serial.println("[SENS] RANDOM mode");
  }

  while(true){
    // เอาค่าเดิมมาก่อน เผื่ออ่านรอบนี้ fail จะไม่เด้งเป็น 0
    SensorData d = STATE.getSensor();

    if(g_useRandom){
      d.soil = random(0, 101);
      d.temp = 20.0f + (random(0, 200) / 10.0f);
      d.airTemp = 25.0f + (random(0, 100) / 10.0f);
      d.airHum  = 40.0f + (random(0, 500) / 10.0f);
    } else {
      bool ok1 = sMoist.read();
      bool ok2 = sTemp.read();
      //bool ok3 = sEC.read();

      if(ok1) d.soil = (float)sMoist.get() / 10.0f;
      else    Serial.println("[MODBUS] moist read FAIL");

      if(ok2) d.temp = (float)sTemp.get() / 10.0f;
      else    Serial.println("[MODBUS] temp read FAIL");

      float at, ah;
      readSht31(at, ah);
      if(!isnan(at) && !isnan(ah)){
        d.airTemp = at;
        d.airHum  = ah;
      } else {
        Serial.println("[SHT31] read FAIL");
      }
    }

    // ✅ สำคัญสุดสำหรับ ControlTask: ต้องอัปเดต tsMs เสมอ
    d.tsMs = millis();
    STATE.setSensor(d);

    Serial.printf("[SENSOR] soil=%.1f temp=%.1f airT=%.1f airH=%.1f tsMs=%lu\n",
      d.soil, d.temp, d.airTemp, d.airHum, (unsigned long)d.tsMs
    );

    vTaskDelay(pdMS_TO_TICKS(SENSOR_INTERVAL_MS));
  }
}

void startSensorTask(bool useRandom, uint32_t stackWords, UBaseType_t prio){
  g_useRandom = useRandom; // ✅ แก้บั๊กสำคัญ
  xTaskCreatePinnedToCore(sensorTask, "SensorTask", stackWords, nullptr, prio, nullptr, 1);
}