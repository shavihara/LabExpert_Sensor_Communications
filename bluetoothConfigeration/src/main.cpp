#include <Arduino.h>
#include <NimBLEDevice.h>
#include <NimBLEServer.h>
#include <Preferences.h>
#include <WiFi.h>

#define BTN_PIN 0

static const char *NS = "prov";
static const char *KEY_SSID = "ssid";
static const char *KEY_PASS = "pass";

Preferences prefs;
NimBLEServer *server = nullptr;
NimBLECharacteristic *tx = nullptr;
NimBLECharacteristic *rx = nullptr;

static constexpr size_t SSID_MAX = 32;   // per 802.11
static constexpr size_t PASS_MAX = 64;   // WPA2 PSK max length
char g_ssid[SSID_MAX + 1] = {0};
char g_pass[PASS_MAX + 1] = {0};
volatile bool confirmed = false;
bool bleOn = false;

static void makeMacSuffix(char *out, size_t outLen) {
  uint8_t mac[6] = {0};
  WiFi.macAddress(mac);
  char cleaned[13] = {0};
  // build 12-char uppercase hex without colons
  static const char *hex = "0123456789ABCDEF";
  for (int i = 0; i < 6; ++i) {
    cleaned[i * 2] = hex[(mac[i] >> 4) & 0xF];
    cleaned[i * 2 + 1] = hex[mac[i] & 0xF];
  }
  const size_t clen = 12;
  const size_t take = clen >= 5 ? 5 : clen;
  const size_t start = clen - take;
  size_t n = (take < outLen - 1) ? take : (outLen - 1);
  memcpy(out, &cleaned[start], n);
  out[n] = '\0';
}

static void notify(const char *s) {
  tx->setValue((uint8_t *)s, strlen(s));
  tx->notify();
}

static void notifyEcho() {
  char buf[16 + SSID_MAX + PASS_MAX];
  int n = snprintf(buf, sizeof(buf), "ECHO:SSID=%s;PASS=%s", g_ssid, g_pass);
  if (n > 0) notify(buf);
}

class RXCB : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *c) {
    const std::string &v = c->getValue();
    if (v.compare(0, 5, "MODE:") == 0) {
      // reserved for future
    } else if (v.compare(0, 5, "SSID:") == 0) {
      size_t len = v.size() - 5;
      if (len > SSID_MAX) len = SSID_MAX;
      memcpy(g_ssid, v.data() + 5, len);
      g_ssid[len] = '\0';
      notifyEcho();
    } else if (v.compare(0, 5, "PASS:") == 0) {
      size_t len = v.size() - 5;
      if (len > PASS_MAX) len = PASS_MAX;
      memcpy(g_pass, v.data() + 5, len);
      g_pass[len] = '\0';
      notifyEcho();
    } else if (v == "CONFIRM") {
      confirmed = true;
    }
  }
};

static RXCB g_rxcb; // static to avoid heap allocation

void startBLE() {
  if (bleOn)
    return;
  bleOn = true;
  char suffix[8] = {0};
  makeMacSuffix(suffix, sizeof(suffix));
  char name[16];
  snprintf(name, sizeof(name), "ESP32-%s", suffix);
  NimBLEDevice::init(name);
  server = NimBLEDevice::createServer();
  NimBLEService *svc =
      server->createService("6E400001-B5A3-F393-E0A9-E50E24DCCA9E");
  tx = svc->createCharacteristic("6E400003-B5A3-F393-E0A9-E50E24DCCA9E",
                                 NIMBLE_PROPERTY::NOTIFY);
  rx = svc->createCharacteristic("6E400002-B5A3-F393-E0A9-E50E24DCCA9E",
                                 NIMBLE_PROPERTY::WRITE);
  rx->setCallbacks(&g_rxcb);
  svc->start();
  NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
  adv->addServiceUUID("6E400001-B5A3-F393-E0A9-E50E24DCCA9E");
  adv->start();
}

void stopBLE() {
  NimBLEDevice::getAdvertising()->stop();
  NimBLEDevice::deinit(true);
  bleOn = false;
}

void connectWiFi() {
  notify("STATUS:CONNECTING");
  WiFi.begin(g_ssid, g_pass);
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000) {
    delay(200);
  }
  if (WiFi.status() == WL_CONNECTED) {
    prefs.begin(NS, false);
    prefs.putString(KEY_SSID, g_ssid);
    prefs.putString(KEY_PASS, g_pass);
    prefs.end();
    notify("STATUS:SUCCESS");
    stopBLE();
  } else {
    notify("STATUS:ERROR:WIFI");
  }
}

void setup() {
  pinMode(BTN_PIN, INPUT_PULLUP);
  WiFi.mode(WIFI_STA);
  prefs.begin(NS, true);
  prefs.getString(KEY_SSID, g_ssid, sizeof(g_ssid));
  prefs.getString(KEY_PASS, g_pass, sizeof(g_pass));
  prefs.end();
  if (g_ssid[0] != '\0' && g_pass[0] != '\0') {
    WiFi.begin(g_ssid, g_pass);
  }
}

void loop() {
  if (digitalRead(BTN_PIN) == LOW) {
    startBLE();
  }
  if (bleOn && confirmed && g_ssid[0] != '\0' && g_pass[0] != '\0') {
    confirmed = false;
    connectWiFi();
  }
}