#include "NetpieConfig.h"
#include <Preferences.h>
#include "AppConfig.h"

static const char* NS_NETPIE = "netpie";

void loadNetpieCreds(NetpieCreds& out){
  Preferences prefs;
  prefs.begin(NS_NETPIE, true);

  out.clientId = prefs.getString("client_id", "");
  out.token    = prefs.getString("token", "");
  out.secret   = prefs.getString("secret", "");

  prefs.end();

  if(out.clientId.length() == 0) out.clientId = DEFAULT_NETPIE_CLIENT_ID;
  if(out.token.length()    == 0) out.token    = DEFAULT_NETPIE_TOKEN;
  if(out.secret.length()   == 0) out.secret   = DEFAULT_NETPIE_SECRET;
}

void saveNetpieCreds(const String& clientId, const String& token, const String& secret){
  Preferences prefs;
  prefs.begin(NS_NETPIE, false);

  prefs.putString("client_id", clientId);
  prefs.putString("token", token);
  prefs.putString("secret", secret);

  prefs.end();
}