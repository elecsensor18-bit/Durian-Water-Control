#include "WifiManager.h"
#include "NetpieConfig.h"

#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

/* ===================== CONFIG ===================== */
#define WIFI_CONNECT_TIMEOUT_MS   15000
#define WIFI_RETRY_INTERVAL_MS     5000
#define WIFI_TASK_TICK_MS           500

#define LED_WIFI_PIN  0            
#define AP_SSID "DurianWaterControl_Setup"
#define AP_PASS "12345678"
#define RESET_AP_PIN  0

/* ===================== FORWARD DECL ===================== */
static void handleRoot();
static void handleSave();
static void handleScan();

static void scanWiFiNetworks();
static void loadWiFiCredential();
static void loadNetpieCredentialForForm();
static void startWebServer();
static void startAPMode();
static void connectToWiFi();
static void updateStatusConnected();
static void updateStatusDisconnected();
static void wifiLoopOnce();
static String htmlEscape(const String& s);

/* ===================== GLOBALS ===================== */
WebServer server(80);
Preferences prefs;

WiFiStatus wifiStatus;

static WiFiState wifiState = WIFI_TRY_CONNECT;
static String wifiListHTML = "";

static String savedSSID = "";
static String savedPASS = "";

static String savedClientId = "";
static String savedToken = "";
static String savedSecret = "";

static unsigned long connectStartTime = 0;
static unsigned long lastRetryMs = 0;

static WiFiOnConnectedCb onConnectedCb = nullptr;

/* ===================== HELPERS ===================== */
static String htmlEscape(const String& s){
  String out;
  out.reserve(s.length() + 16);
  for(size_t i=0;i<s.length();++i){
    char c = s[i];
    switch(c){
      case '&': out += "&amp;";  break;
      case '<': out += "&lt;";   break;
      case '>': out += "&gt;";   break;
      case '"': out += "&quot;"; break;
      case '\'': out += "&#39;"; break;
      default: out += c; break;
    }
  }
  return out;
}

static void scanWiFiNetworks() {
  int n = WiFi.scanNetworks();
  wifiListHTML = "";

  if (n <= 0) {
    wifiListHTML = "<option value=''>No networks found</option>";
    return;
  }

  for (int i = 0; i < n; ++i) {
    String ssid = WiFi.SSID(i);
    int rssi = WiFi.RSSI(i);

    wifiListHTML += "<option value='";
    wifiListHTML += htmlEscape(ssid);
    wifiListHTML += "'";

    if (ssid == savedSSID) {
      wifiListHTML += " selected";
    }

    wifiListHTML += ">";
    wifiListHTML += htmlEscape(ssid);
    wifiListHTML += " (";
    wifiListHTML += String(rssi);
    wifiListHTML += " dBm)";
    wifiListHTML += "</option>";
  }
}

void loadWiFiCredential() {
  prefs.begin("wifi", true);
  savedSSID = prefs.getString("ssid", "");
  savedPASS = prefs.getString("pass", "");
  prefs.end();

  Serial.println("[WIFI] Loaded credentials");
  Serial.println("[WIFI] SSID: " + savedSSID);
}

static void loadNetpieCredentialForForm() {
  NetpieCreds c;
  loadNetpieCreds(c);
  savedClientId = c.clientId;
  savedToken    = c.token;
  savedSecret   = c.secret;
}

/* ===================== HTTP HANDLERS ===================== */
static void handleRoot() {
  loadWiFiCredential();
  loadNetpieCredentialForForm();

  if (wifiListHTML.length() == 0) {
    scanWiFiNetworks();
  }

  String apIp = WiFi.softAPIP().toString();

  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="th">
<head>
<meta charset="utf-8" />
<meta name="viewport" content="width=device-width, initial-scale=1" />
<title>Wi-Fi Setup</title>
<style>
  :root{
    --bg:#0b1220;
    --card:#0f1a30;
    --txt:#eaf0ff;
    --muted:#a9b6d6;
    --line:rgba(255,255,255,.08);
    --accent:#4da3ff;
    --accent2:#37d7a5;
    --shadow:0 10px 30px rgba(0,0,0,.35);
    --radius:16px;
  }
  *{box-sizing:border-box}
  body{
    margin:0;
    font-family:ui-sans-serif,system-ui,-apple-system,Segoe UI,Roboto,Arial,"Noto Sans Thai",sans-serif;
    background:
      radial-gradient(1200px 800px at 20% 0%, rgba(77,163,255,.20), transparent 60%),
      radial-gradient(900px 700px at 80% 20%, rgba(55,215,165,.14), transparent 55%),
      var(--bg);
    color:var(--txt);
    min-height:100vh;
    display:flex;
    align-items:center;
    justify-content:center;
    padding:24px;
  }
  .wrap{width:min(980px,100%)}
  .top{
    display:flex;gap:14px;align-items:center;justify-content:space-between;
    margin-bottom:14px;
  }
  .brand{display:flex;gap:12px;align-items:center}
  .logo{
    width:42px;height:42px;border-radius:12px;
    background:linear-gradient(135deg, rgba(77,163,255,.95), rgba(55,215,165,.95));
    box-shadow:var(--shadow);
  }
  .title{line-height:1.15}
  .title h1{margin:0;font-size:18px}
  .title p{margin:2px 0 0;color:var(--muted);font-size:13px}
  .pill{
    font-size:12px;color:var(--muted);
    border:1px solid var(--line);border-radius:999px;
    padding:8px 12px;background:rgba(255,255,255,.03);
    display:flex;gap:8px;align-items:center;
  }
  .pill b{color:var(--txt);font-weight:600}
  .grid{
    display:grid;
    grid-template-columns:1.2fr .8fr;
    gap:14px;
  }
  @media (max-width: 860px){
    .grid{grid-template-columns:1fr}
    .top{flex-direction:column;align-items:flex-start}
  }
  .card{
    background:linear-gradient(180deg, rgba(255,255,255,.04), rgba(255,255,255,.02));
    border:1px solid var(--line);
    border-radius:var(--radius);
    box-shadow:var(--shadow);
    overflow:hidden;
  }
  .head{
    padding:16px 18px;
    background:linear-gradient(180deg, rgba(255,255,255,.06), rgba(255,255,255,0));
    border-bottom:1px solid var(--line);
  }
  .head h2{margin:0;font-size:15px}
  .head p{margin:6px 0 0;color:var(--muted);font-size:13px}
  .body{padding:16px 18px}
  label{display:block;margin:10px 0 6px;color:var(--muted);font-size:13px}
  select,input{
    width:100%;
    padding:12px 12px;
    border-radius:12px;
    border:1px solid var(--line);
    background:rgba(255,255,255,.03);
    color:var(--txt);
    outline:none;
  }
  select:focus,input:focus{
    border-color:rgba(77,163,255,.55);
    box-shadow:0 0 0 4px rgba(77,163,255,.12);
  }
  .row{display:flex;gap:10px;align-items:center}
  .row>*{flex:1}
  .btn{
    cursor:pointer;
    padding:12px 14px;
    border-radius:12px;
    border:1px solid var(--line);
    background:rgba(255,255,255,.04);
    color:var(--txt);
    font-weight:600;
    transition:.15s transform,.15s background;
  }
  .btn:hover{background:rgba(255,255,255,.07)}
  .btn:active{transform:translateY(1px)}
  .btn.primary{
    border-color:rgba(77,163,255,.45);
    background:linear-gradient(135deg, rgba(77,163,255,.9), rgba(77,163,255,.55));
  }
  .btn.good{
    border-color:rgba(55,215,165,.45);
    background:linear-gradient(135deg, rgba(55,215,165,.9), rgba(55,215,165,.55));
  }
  .divider{height:1px;background:var(--line);margin:14px 0}
  .hint{margin-top:10px;color:var(--muted);font-size:12px;line-height:1.5}
  .small{font-size:12px;color:var(--muted)}
  .kvs{display:grid;grid-template-columns:120px 1fr;gap:8px 12px;font-size:13px}
  .k{color:var(--muted)}
  .toast{
    display:none;
    margin-top:12px;
    padding:10px 12px;
    border-radius:12px;
    border:1px solid var(--line);
    background:rgba(255,255,255,.03);
  }
  .footer{
    margin-top:12px;
    color:rgba(255,255,255,.45);
    font-size:11px;
    text-align:center;
  }
</style>
</head>
<body>
<div class="wrap">

  <div class="top">
    <div class="brand">
      <div class="logo"></div>
      <div class="title">
        <h1>ตั้งค่า Wi-Fi และ NETPIE</h1>
        <p>กำหนดค่าการเชื่อมต่อของอุปกรณ์</p>
      </div>
    </div>
    <div class="pill">AP: <b>)rawliteral";

  html += AP_SSID;

  html += R"rawliteral(</b> • IP: <b>)rawliteral";
  html += apIp;
  html += R"rawliteral(</b></div>
  </div>

  <div class="grid">
    <div class="card">
      <div class="head">
        <h2>ตั้งค่าการเชื่อมต่อ</h2>
        <p>กรอกค่าให้ครบ แล้วกดบันทึกเพื่อรีสตาร์ตอุปกรณ์</p>
      </div>
      <div class="body">
        <form id="f" action="/save" method="POST">

          <label>Wi-Fi Network (SSID)</label>
          <div class="row">
            <select name="ssid" id="ssidSelect" required>
)rawliteral";

  html += wifiListHTML;

  html += R"rawliteral(
            </select>
            <button class="btn" type="button" onclick="location.href='/scan'">รีเฟรช</button>
          </div>

          <label>Password</label>
          <div class="row">
            <input type="password" name="pass" id="pass" placeholder="ใส่รหัสผ่าน Wi-Fi" value=")rawliteral";
  html += htmlEscape(savedPASS);
  html += R"rawliteral(" autocomplete="current-password" />
            <button class="btn" type="button" onclick="togglePw('pass')">แสดง/ซ่อน</button>
          </div>

          <div class="divider"></div>

          <label>NETPIE Client ID</label>
          <input type="text" name="client_id" id="client_id" value=")rawliteral";
  html += htmlEscape(savedClientId);
  html += R"rawliteral(" placeholder="Client ID" />

          <label>NETPIE Token</label>
          <div class="row">
            <input type="password" name="token" id="token" value=")rawliteral";
  html += htmlEscape(savedToken);
  html += R"rawliteral(" placeholder="Token" />
            <button class="btn" type="button" onclick="togglePw('token')">แสดง/ซ่อน</button>
          </div>

          <label>NETPIE Secret</label>
          <div class="row">
            <input type="password" name="secret" id="secret" value=")rawliteral";
  html += htmlEscape(savedSecret);
  html += R"rawliteral(" placeholder="Secret" />
            <button class="btn" type="button" onclick="togglePw('secret')">แสดง/ซ่อน</button>
          </div>

          <div class="divider"></div>

          <div class="row">
            <button class="btn primary" type="submit" onclick="showSaving()">💾 บันทึก & รีสตาร์ต</button>
            <button class="btn good" type="button" onclick="showHint()">ℹ️ ช่วยเหลือ</button>
          </div>

          <div id="toast" class="toast"></div>

          <div class="hint">
            • ถ้าไม่เห็น SSID ให้กด “รีเฟรช”<br>
            • ถ้าต่อ Wi-Fi ไม่ติด ให้ตรวจสอบรหัสผ่าน<br>
            • สามารถกดปุ่ม Reset/AP เพื่อกลับเข้าโหมดตั้งค่าได้
          </div>
        </form>
      </div>
    </div>

    <div class="card">
      <div class="head">
        <h2>ข้อมูลอุปกรณ์</h2>
        <p>ใช้สำหรับเข้าหน้าตั้งค่าและตรวจสอบสถานะ</p>
      </div>
      <div class="body">
        <div class="kvs">
          <div class="k">AP SSID</div><div>)rawliteral";
  html += AP_SSID;
  html += R"rawliteral(</div>

          <div class="k">AP IP</div><div>)rawliteral";
  html += apIp;
  html += R"rawliteral(</div>

          <div class="k">URL</div><div>http://)rawliteral";
  html += apIp;
  html += R"rawliteral(/</div>
        </div>

        <div class="divider"></div>

        <div class="small">
          หน้านี้ทำงานบน Access Point ของอุปกรณ์เอง ไม่ต้องใช้อินเทอร์เน็ต
        </div>
      </div>
    </div>
  </div>

  <div class="footer">Carbon Farm • Setup Portal</div>
</div>

<script>
function togglePw(id){
  const el = document.getElementById(id);
  el.type = (el.type === 'password') ? 'text' : 'password';
}
function showSaving(){
  const t = document.getElementById('toast');
  t.style.display = 'block';
  t.innerHTML = 'กำลังบันทึกค่า... อุปกรณ์จะรีสตาร์ตอัตโนมัติ';
}
function showHint(){
  const t = document.getElementById('toast');
  t.style.display = 'block';
  t.innerHTML = 'ทิป: กรอก Wi-Fi และ NETPIE ให้ถูกต้อง แล้วกดบันทึก';
}
</script>
</body>
</html>
)rawliteral";

  server.send(200, "text/html", html);
}

static void handleSave() {
  String ssid = server.arg("ssid");
  String pass = server.arg("pass");

  String clientId = server.arg("client_id");
  String token    = server.arg("token");
  String secret   = server.arg("secret");

  Serial.println("[WIFI] Received new config");
  Serial.println("[WIFI] SSID: " + ssid);
  Serial.println("[NETPIE] Client ID: " + clientId);

  prefs.begin("wifi", false);
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  prefs.end();

  saveNetpieCreds(clientId, token, secret);

  server.send(200, "text/html", "<h3>Saved! Rebooting...</h3>");
  delay(1200);
  ESP.restart();
}

static void handleScan() {
  scanWiFiNetworks();
  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "OK");
}

void startWebServer() {
  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/scan", HTTP_GET, handleScan);
  server.begin();

  Serial.println("🌐 Web Server Started");
}

/* ===================== WIFI CONTROL ===================== */
static void startAPMode() {
  WiFi.disconnect(true);
  delay(200);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);

  scanWiFiNetworks();

  Serial.println("[WIFI] AP MODE started");
  Serial.print("[WIFI] AP IP: ");
  Serial.println(WiFi.softAPIP());

  startWebServer();

  wifiState = WIFI_AP_MODE;
  wifiStatus.state = WIFI_AP_MODE;
  wifiStatus.connected = false;
  strncpy(wifiStatus.ip, "0.0.0.0", sizeof(wifiStatus.ip));
  wifiStatus.rssi = 0;

  digitalWrite(LED_WIFI_PIN, HIGH);
}

static void connectToWiFi() {
  loadWiFiCredential();

  if (savedSSID.length() == 0) {
    Serial.println("[WIFI] No saved SSID -> AP mode");
    startAPMode();
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(savedSSID.c_str(), savedPASS.c_str());

  Serial.println("[WIFI] Connecting STA...");
  Serial.println("[WIFI] SSID: " + savedSSID);

  connectStartTime = millis();
  wifiState = WIFI_TRY_CONNECT;
  wifiStatus.state = WIFI_TRY_CONNECT;
}

bool wifiIsConnected() {
  return WiFi.status() == WL_CONNECTED;
}

String wifiGetSSID() {
  return WiFi.SSID();
}

void wifiBegin(WiFiOnConnectedCb cb) {
  onConnectedCb = cb;

  pinMode(LED_WIFI_PIN, OUTPUT);
  pinMode(RESET_AP_PIN, INPUT_PULLUP);

  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);

  connectToWiFi();
}

static void updateStatusConnected() {
  wifiStatus.connected = true;
  wifiStatus.state = WIFI_CONNECTED;
  wifiState = WIFI_CONNECTED;

  IPAddress ip = WiFi.localIP();
  snprintf(wifiStatus.ip, sizeof(wifiStatus.ip), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);

  wifiStatus.rssi = WiFi.RSSI();

  Serial.print("[WIFI] ✅ Connected IP=");
  Serial.println(wifiStatus.ip);

  digitalWrite(LED_WIFI_PIN, LOW);

  if (onConnectedCb) onConnectedCb();
}

static void updateStatusDisconnected() {
  wifiStatus.connected = false;
  wifiStatus.state = WIFI_TRY_CONNECT;
  strncpy(wifiStatus.ip, "0.0.0.0", sizeof(wifiStatus.ip));
  wifiStatus.rssi = 0;
}

static void wifiLoopOnce() {
  if (digitalRead(RESET_AP_PIN) == LOW) {
    Serial.println("[WIFI] Button pressed -> AP mode");
    startAPMode();
    return;
  }

  switch (wifiState) {
    case WIFI_TRY_CONNECT: {
      digitalWrite(LED_WIFI_PIN, !digitalRead(LED_WIFI_PIN));

      if (WiFi.status() == WL_CONNECTED) {
        updateStatusConnected();
        return;
      }

      if (millis() - connectStartTime > WIFI_CONNECT_TIMEOUT_MS) {
        Serial.println("[WIFI] connect timeout -> retry");
        updateStatusDisconnected();

        if (millis() - lastRetryMs > WIFI_RETRY_INTERVAL_MS) {
          lastRetryMs = millis();
          connectToWiFi();
        }
      }
    } break;

    case WIFI_CONNECTED: {
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WIFI] ⚠️ Lost -> reconnect");
        updateStatusDisconnected();
        connectToWiFi();
      } else {
        wifiStatus.rssi = WiFi.RSSI();
      }
    } break;

    case WIFI_AP_MODE: {
      server.handleClient();
      digitalWrite(LED_WIFI_PIN, HIGH);
    } break;
  }
}

static void wifiTask(void*) {
  while (true) {
    wifiLoopOnce();
    vTaskDelay(pdMS_TO_TICKS(WIFI_TASK_TICK_MS));
  }
}

void startWiFiTask(uint32_t stackWords, UBaseType_t prio) {
  xTaskCreatePinnedToCore(wifiTask, "WiFiTask", stackWords, nullptr, prio, nullptr, 0);
}