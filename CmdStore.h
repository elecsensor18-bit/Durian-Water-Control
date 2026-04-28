#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include "CmdStruct.h"

class CmdStore {
public:
  static void begin();
  static bool load(CmdStruct& out);
  static bool save(const CmdStruct& cmd); // call only when cmd.id changed

private:
  static Preferences _prefs;
};
