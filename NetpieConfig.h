#pragma once
#include <Arduino.h>

struct NetpieCreds {
  String clientId;
  String token;
  String secret;
};

void loadNetpieCreds(NetpieCreds& out);
void saveNetpieCreds(const String& clientId, const String& token, const String& secret);