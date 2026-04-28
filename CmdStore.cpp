#include "CmdStore.h"
Preferences CmdStore::_prefs;

static void putU8(Preferences& p, const char* k, uint8_t v){ p.putUChar(k, v); }
static uint8_t getU8(Preferences& p, const char* k, uint8_t d){ return p.getUChar(k, d); }

void CmdStore::begin(){ _prefs.begin("carbonfarm", false); }

bool CmdStore::save(const CmdStruct& c){
  _prefs.putString("id", c.id);
  _prefs.putUInt("ts", c.ts);

  putU8(_prefs,"mode",(uint8_t)c.mode);
  putU8(_prefs,"tgt", c.targetsMask);
  putU8(_prefs,"man", c.manualMask);

  _prefs.putFloat("min", c.minVal);
  _prefs.putFloat("max", c.maxVal);
  _prefs.putFloat("hys", c.hyst);

  putU8(_prefs,"wcnt", c.windowCount);
  for(uint8_t i=0;i<CmdStruct::MAX_WINDOWS;i++){
    char k[16];
    snprintf(k,sizeof(k),"w%u_en",i); _prefs.putBool(k, c.windows[i].en);
    snprintf(k,sizeof(k),"w%u_dm",i); _prefs.putUChar(k, c.windows[i].daysMask);
    snprintf(k,sizeof(k),"w%u_st",i); _prefs.putUShort(k, c.windows[i].startMin);
    snprintf(k,sizeof(k),"w%u_du",i); _prefs.putUShort(k, c.windows[i].durationMin);
    snprintf(k,sizeof(k),"w%u_rm",i); _prefs.putUChar(k, c.windows[i].relayMask);
  }

  _prefs.putBool("fs_off", c.failSafeOff);
  _prefs.putUShort("sto", c.sensorTimeoutSec);
  return true;
}

bool CmdStore::load(CmdStruct& c){
  String id = _prefs.getString("id", "");
  if(id.length()==0) return false;

  c.id = id;
  c.ts = _prefs.getUInt("ts", 0);

  c.mode = (ControlMode)getU8(_prefs,"mode",(uint8_t)ControlMode::MANUAL);
  c.targetsMask = getU8(_prefs,"tgt",0x0F);
  c.manualMask  = getU8(_prefs,"man",0);

  c.minVal = _prefs.getFloat("min", 0);
  c.maxVal = _prefs.getFloat("max", 100);
  c.hyst   = _prefs.getFloat("hys", 0);

  c.windowCount = getU8(_prefs,"wcnt",0);
  for(uint8_t i=0;i<CmdStruct::MAX_WINDOWS;i++){
    char k[16];
    snprintf(k,sizeof(k),"w%u_en",i); c.windows[i].en = _prefs.getBool(k,false);
    snprintf(k,sizeof(k),"w%u_dm",i); c.windows[i].daysMask = _prefs.getUChar(k,0);
    snprintf(k,sizeof(k),"w%u_st",i); c.windows[i].startMin = _prefs.getUShort(k,65535);
    snprintf(k,sizeof(k),"w%u_du",i); c.windows[i].durationMin = _prefs.getUShort(k,0);
    snprintf(k,sizeof(k),"w%u_rm",i); c.windows[i].relayMask = _prefs.getUChar(k,0);
  }

  c.failSafeOff = _prefs.getBool("fs_off", true);
  c.sensorTimeoutSec = _prefs.getUShort("sto", 300);
  return true;
}
