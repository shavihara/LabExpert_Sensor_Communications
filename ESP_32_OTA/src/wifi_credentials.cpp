#include "wifi_credentials.h"
#include <WiFi.h>
#include <esp_system.h>
#include <esp_event.h>
#include <esp_wifi.h>
#include <nvs_flash.h>
#include <nvs.h>

#ifdef ARDUINO_ARCH_ESP32
#include <esp_mac.h>
#endif

#if defined(WCM_USE_NIMBLE) && WCM_USE_NIMBLE
#include <NimBLEDevice.h>
#include <mbedtls/aes.h>
#include <mbedtls/sha256.h>
#else
#include <BluetoothSerial.h>
static BluetoothSerial SerialBT;
#endif

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <vector>

// PROGMEM static strings
static const char kNamespace[] = "wifi";
static const char kKeySsid[]   = "ssid";
static const char kKeyPass[]   = "pass";
static const char kKeyHostMac[] = "hostmac";
static const char kDevPrefix[] = "LabExpertOTA";
static const uint8_t kBleSecret[] = { 'D','E','V','_','S','E','C','R','E','T' };

// Event bits
static const EventBits_t EVT_WIFI_GOT_IP    = BIT0;
static const EventBits_t EVT_PROV_START     = BIT1;
static const EventBits_t EVT_PROV_COMMIT    = BIT2;
static const EventBits_t EVT_PROV_DONE      = BIT3;

// Button pin
#ifndef WCM_BUTTON_PIN
#define WCM_BUTTON_PIN 34
#endif

// Forward static wrappers
static EventGroupHandle_t sEvtGroup = nullptr;
static TaskHandle_t sBtnTask = nullptr;
static TaskHandle_t sProvTask = nullptr;
static WiFiCredentialManager* sMgr = nullptr;

// Helpers
static inline bool isValidSSIDChar(char c) {
  return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '.' || c == '_' || c == '-';
}
static inline bool isValidPASSChar(char c) {
  return c >= 0x20 && c <= 0x7E && c != '"' && c != '`';
}

static bool validateSSID(const char* s) {
  size_t n = strnlen(s, 33);
  if (n < 1 || n > 32) return false;
  for (size_t i = 0; i < n; ++i) if (!isValidSSIDChar(s[i])) return false;
  return true;
}
static bool validatePASS(const char* p) {
  size_t n = strnlen(p, 65);
  if (n < 8 || n > 64) return false;
  for (size_t i = 0; i < n; ++i) if (!isValidPASSChar(p[i])) return false;
  return true;
}

static void maskPass(const char* in, char* out, size_t outSize) {
  size_t n = strnlen(in, 65);
  size_t m = (n < outSize - 1) ? n : (outSize - 1);
  for (size_t i = 0; i < m; ++i) out[i] = '*';
  out[m] = '\0';
}

// Memory monitoring helpers
static size_t getFreeHeap() { return esp_get_free_heap_size(); }
static size_t getMinFreeHeap() { return esp_get_minimum_free_heap_size(); }

static void sha256(const uint8_t* in, size_t n, uint8_t out[32]);
static void aes_cbc_decrypt(const uint8_t* key, const uint8_t* iv_ct, size_t len, std::vector<uint8_t>& out);

// BLE callbacks (NimBLE)
#if defined(WCM_USE_NIMBLE) && WCM_USE_NIMBLE
class WcmSsidCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c) override {
    std::string v = c->getValue();
    size_t n = v.size();
    if (n > 32) n = 32;
    memset(sMgr->ssidBuf, 0, sizeof(sMgr->ssidBuf));
    for (size_t i = 0; i < n; ++i) sMgr->ssidBuf[i] = (char)v[i];
  }
};

class WcmPassCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c) override {
    std::string v = c->getValue();
    size_t n = v.size();
    if (n > 64) n = 64;
    memset(sMgr->passBuf, 0, sizeof(sMgr->passBuf));
    for (size_t i = 0; i < n; ++i) sMgr->passBuf[i] = (char)v[i];
  }
};

class WcmCommitCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c) override {
    // Any write triggers commit attempt
    xEventGroupSetBits(sEvtGroup, EVT_PROV_COMMIT);
  }
};

class WcmHostMacCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c) override {
    std::string v = c->getValue();
    size_t n = v.size();
    if (n > 17) n = 17;
    memset(sMgr->hostMacBuf, 0, sizeof(sMgr->hostMacBuf));
    for (size_t i = 0; i < n; ++i) sMgr->hostMacBuf[i] = (char)v[i];
    Serial.printf("Received HOST MAC: %s\n", sMgr->hostMacBuf);
  }
};

static NimBLECharacteristic* sStatusChar = nullptr;
static WcmSsidCallbacks sSsidCb;
static WcmPassCallbacks sPassCb;
static WcmCommitCallbacks sCommitCb;
static WcmHostMacCallbacks sHostMacCb;
#endif

// No WiFi event callback; using status polling to reduce API differences

// Button task
static void buttonTaskThunk(void* arg) {
  pinMode(WCM_BUTTON_PIN, INPUT); // External 10K pull-up used (pin 34 doesn't support internal pull-up)
  const TickType_t sample = pdMS_TO_TICKS(10);
  uint32_t pressedMs = 0;
  bool last = digitalRead(WCM_BUTTON_PIN) == LOW;
  for (;;) {
    bool cur = digitalRead(WCM_BUTTON_PIN) == LOW;
    if (cur) {
      pressedMs += 10;
      if (pressedMs >= 3000) {
        Serial.println("🔘 Button held for 3s - Erasing credentials and restarting...");
        
        // Clear credentials using the manager's method
        if (sMgr) {
          sMgr->clearCredentials();
          Serial.println("✓ Credentials erased");
        }

        // Disconnect WiFi and restart ESP32 to boot into provisioning mode
        WiFi.disconnect(true, true);
        delay(500);
        Serial.println("🔄 Rebooting to enter provisioning mode...");
        ESP.restart();
        
        pressedMs = 0; // prevent repeat triggers (though we're rebooting anyway)
      }
    } else {
      pressedMs = 0;
    }
    last = cur;
    vTaskDelay(sample);
  }
}

// Provisioning task
void provTaskThunk(void* arg) {
  WiFiCredentialManager* mgr = (WiFiCredentialManager*)arg;
  mgr->handleBluetoothProvisioning();
  vTaskDelete(nullptr);
}

bool WiFiCredentialManager::begin() {
  sMgr = this;
  memset(ssidBuf, 0, sizeof(ssidBuf));
  memset(passBuf, 0, sizeof(passBuf));
  memset(hostMacBuf, 0, sizeof(hostMacBuf));

  // Init NVS
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    nvs_flash_erase();
    err = nvs_flash_init();
  }
  if (err != ESP_OK) return false;

  sEvtGroup = xEventGroupCreate();
  if (!sEvtGroup) return false;

  // Optional event hook; connection wait uses polling to reduce API dependencies

  startButtonTask();

  // If no credentials, start provisioning immediately
  if (!checkSavedCredentials()) {
    startProvisioningTask();
  }

  return true;
}

bool WiFiCredentialManager::connectWiFi() {
  if (!checkSavedCredentials()) return false;

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssidBuf, passBuf);

  // Poll status to ensure compatibility across Arduino core versions
  TickType_t start = xTaskGetTickCount();
  while ((xTaskGetTickCount() - start) < pdMS_TO_TICKS(15000)) {
    if (WiFi.status() == WL_CONNECTED) {
      return true;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
  return false;
}

bool WiFiCredentialManager::checkSavedCredentials() {
  nvs_handle_t h;
  if (nvs_open(kNamespace, NVS_READONLY, &h) != ESP_OK) {
    return false;
  }
  size_t ss = sizeof(ssidBuf);
  size_t sp = sizeof(passBuf);
  size_t sm = sizeof(hostMacBuf);
  esp_err_t ers = nvs_get_str(h, kKeySsid, ssidBuf, &ss);
  esp_err_t erp = nvs_get_str(h, kKeyPass, passBuf, &sp);
  
  // Load Host MAC if available (best effort)
  if (nvs_get_str(h, kKeyHostMac, hostMacBuf, &sm) != ESP_OK) {
    memset(hostMacBuf, 0, sizeof(hostMacBuf));
  }

  nvs_close(h);
  if (ers != ESP_OK || erp != ESP_OK) return false;
  if (!validateSSID(ssidBuf) || !validatePASS(passBuf)) return false;
  return true;
}

void WiFiCredentialManager::clearCredentials() {
  // Erase from NVS
  nvs_handle_t h;
  if (nvs_open(kNamespace, NVS_READWRITE, &h) == ESP_OK) {
    nvs_erase_key(h, kKeySsid);
    nvs_erase_key(h, kKeyPass);
    nvs_erase_key(h, kKeyHostMac);
    nvs_commit(h);
    nvs_close(h);
  }
  
  // Clear internal buffers
  memset(ssidBuf, 0, sizeof(ssidBuf));
  memset(passBuf, 0, sizeof(passBuf));
  memset(hostMacBuf, 0, sizeof(hostMacBuf));
}

void WiFiCredentialManager::startButtonTask() {
  if (!sBtnTask) {
    xTaskCreatePinnedToCore(buttonTaskThunk, "btn", 2048, nullptr, 1, &sBtnTask, 1);
  }
}

void WiFiCredentialManager::startProvisioningTask() {
  if (!sProvTask) {
    xTaskCreatePinnedToCore(provTaskThunk, "prov", 6144, this, 1, &sProvTask, 1);
  }
}

void WiFiCredentialManager::handleBluetoothProvisioning() {
  // Build device name
  uint8_t mac[6] = {0};
#ifdef ARDUINO_ARCH_ESP32
  esp_read_mac(mac, ESP_MAC_BT);
#endif
  char devName[24];
  snprintf(devName, sizeof(devName), "%s", kDevPrefix);
  size_t len = strnlen(devName, sizeof(devName));
  snprintf(devName + len, sizeof(devName) - len, "%02X%02X%02X%02X%02X", mac[1], mac[2], mac[3], mac[4], mac[5]);

  TickType_t startTick = xTaskGetTickCount();

#if defined(WCM_USE_NIMBLE) && WCM_USE_NIMBLE
  // Start BLE (NimBLE)
  NimBLEDevice::init(devName);
  NimBLEServer* server = NimBLEDevice::createServer();
  NimBLEService* svc = server->createService("0000FFF0-0000-1000-8000-00805F9B34FB");
  NimBLECharacteristic* ssidChar = svc->createCharacteristic("0000FFF1-0000-1000-8000-00805F9B34FB", NIMBLE_PROPERTY::WRITE);
  NimBLECharacteristic* passChar = svc->createCharacteristic("0000FFF2-0000-1000-8000-00805F9B34FB", NIMBLE_PROPERTY::WRITE);
  sStatusChar = svc->createCharacteristic("0000FFF3-0000-1000-8000-00805F9B34FB", NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  NimBLECharacteristic* commitChar = svc->createCharacteristic("0000FFF4-0000-1000-8000-00805F9B34FB", NIMBLE_PROPERTY::WRITE);
  NimBLECharacteristic* hostMacChar = svc->createCharacteristic("0000FFF5-0000-1000-8000-00805F9B34FB", NIMBLE_PROPERTY::WRITE);

  ssidChar->setCallbacks(&sSsidCb);
  passChar->setCallbacks(&sPassCb);
  commitChar->setCallbacks(&sCommitCb);
  hostMacChar->setCallbacks(&sHostMacCb);

  svc->start();
  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->addServiceUUID(svc->getUUID());
  adv->setScanResponse(true);
  adv->start();

  auto nowMs = [](){ return (uint32_t)(xTaskGetTickCount()*portTICK_PERIOD_MS); };
  char statusBuf[96];
  uint32_t startDeadline = nowMs() + 30000;
  for (;;) {
    // Check for provisioning events
    EventBits_t bits = xEventGroupGetBits(sEvtGroup);
    if (bits & EVT_PROV_COMMIT) {
      if (isRateLimited()) {
        sStatusChar->setValue((uint8_t*)"RATE_LIMITED", 12);
        sStatusChar->notify();
        xEventGroupClearBits(sEvtGroup, EVT_PROV_COMMIT);
        continue;
      }
      bool okS = validateSSID(ssidBuf);
      bool okP = validatePASS(passBuf);
      char masked[65];
      maskPass(passBuf, masked, sizeof(masked));
      if (okS && okP) {
        // Store in NVS
        nvs_handle_t h;
        if (nvs_open(kNamespace, NVS_READWRITE, &h) == ESP_OK) {
          esp_err_t ers = nvs_set_str(h, kKeySsid, ssidBuf);
          esp_err_t erp = nvs_set_str(h, kKeyPass, passBuf);
          if (strnlen(hostMacBuf, sizeof(hostMacBuf)) > 0) {
            nvs_set_str(h, kKeyHostMac, hostMacBuf);
          }
          esp_err_t cm = nvs_commit(h);
          nvs_close(h);
          if (ers == ESP_OK && erp == ESP_OK && cm == ESP_OK) {
            snprintf(statusBuf, sizeof(statusBuf), "STORED SSID:%s PASS:%s", ssidBuf, masked);
            sStatusChar->setValue((uint8_t*)statusBuf, strnlen(statusBuf, sizeof(statusBuf)));
            sStatusChar->notify();
            Serial.printf("Saved Host MAC: %s\n", hostMacBuf);
            xEventGroupSetBits(sEvtGroup, EVT_PROV_DONE);
            commitAttempts++;
            if (WiFi.status() != WL_CONNECTED) {
              WiFi.mode(WIFI_STA);
              WiFi.begin(ssidBuf, passBuf);
              TickType_t st = xTaskGetTickCount();
              while ((xTaskGetTickCount()-st) < pdMS_TO_TICKS(15000)) {
                if (WiFi.status()==WL_CONNECTED) break;
                vTaskDelay(pdMS_TO_TICKS(100));
              }
            }
            if (WiFi.status()==WL_CONNECTED) {
              sStatusChar->setValue((uint8_t*)"WIFI_OK", 7);
              sStatusChar->notify();
            } else {
              sStatusChar->setValue((uint8_t*)"WIFI_FAIL", 9);
              sStatusChar->notify();
            }
            
            // Wait a moment for final notification to be sent
            vTaskDelay(pdMS_TO_TICKS(500));
            
            // Restart ESP32 to boot with new credentials
            Serial.println("✓ Credentials saved. Restarting...");
            ESP.restart();
            
            break;
          } else {
            snprintf(statusBuf, sizeof(statusBuf), "NVS_ERR");
            sStatusChar->setValue((uint8_t*)statusBuf, strnlen(statusBuf, sizeof(statusBuf)));
            sStatusChar->notify();
          }
        }
      } else {
        snprintf(statusBuf, sizeof(statusBuf), okS ? "INVALID_PASS" : "INVALID_SSID");
        sStatusChar->setValue((uint8_t*)statusBuf, strnlen(statusBuf, sizeof(statusBuf)));
        sStatusChar->notify();
      }
      // Reset commit bit
      xEventGroupClearBits(sEvtGroup, EVT_PROV_COMMIT);
    }
    if (nowMs() > startDeadline) {
      sStatusChar->setValue((uint8_t*)"TIMEOUT", 7);
      sStatusChar->notify();
      startDeadline = nowMs() + 30000;
    }
    
    // Yield to allow Bluetooth stack to process events
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  NimBLEDevice::stopAdvertising();
  NimBLEDevice::deinit(true);
#else
  // Fallback: Classic Bluetooth SPP (polling)
  SerialBT.begin(devName);
  char inBuf[96];
  size_t idx = 0;
  enum { WAIT, GOT_SSID, GOT_PASS } state = WAIT;
  for (;;) {
    while (SerialBT.available()) {
      char c = (char)SerialBT.read();
      if (c == '\n' || c == '\r') {
        inBuf[idx] = '\0';
        idx = 0;
        if (state == WAIT && strncmp(inBuf, "SSID=", 5) == 0) {
          strncpy(ssidBuf, inBuf + 5, sizeof(ssidBuf) - 1);
          state = GOT_SSID;
          SerialBT.println("OK SSID");
        } else if (state == GOT_SSID && strncmp(inBuf, "PASS=", 5) == 0) {
          strncpy(passBuf, inBuf + 5, sizeof(passBuf) - 1);
          state = GOT_PASS;
          SerialBT.println("OK PASS");
        } else if (state == GOT_PASS && strcmp(inBuf, "COMMIT") == 0) {
          bool okS = validateSSID(ssidBuf);
          bool okP = validatePASS(passBuf);
          if (okS && okP) {
            nvs_handle_t h;
            if (nvs_open(kNamespace, NVS_READWRITE, &h) == ESP_OK) {
              esp_err_t ers = nvs_set_str(h, kKeySsid, ssidBuf);
              esp_err_t erp = nvs_set_str(h, kKeyPass, passBuf);
              esp_err_t cm = nvs_commit(h);
              nvs_close(h);
              if (ers == ESP_OK && erp == ESP_OK && cm == ESP_OK) {
                SerialBT.println("STORED");
                xEventGroupSetBits(sEvtGroup, EVT_PROV_DONE);
                goto endBT;
              } else {
                SerialBT.println("NVS_ERR");
              }
            }
          } else {
            SerialBT.println(okS ? "INVALID_PASS" : "INVALID_SSID");
          }
        }
        memset(inBuf, 0, sizeof(inBuf));
      } else if (idx < sizeof(inBuf) - 1) {
        inBuf[idx++] = c;
      }
    }
    if ((xTaskGetTickCount() - startTick) > pdMS_TO_TICKS(60000)) {
      SerialBT.println("TIMEOUT");
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
endBT:
  SerialBT.end();
#endif

  // Cleanup sensitive buffer
  memset(passBuf, 0, sizeof(passBuf));
}

static void sha256(const uint8_t* in, size_t n, uint8_t out[32]) {
  mbedtls_sha256(in, n, out, 0);
}

static void aes_cbc_decrypt(const uint8_t* key, const uint8_t* iv_ct, size_t len, std::vector<uint8_t>& out) {
  const uint8_t* iv = iv_ct;
  const uint8_t* ct = iv_ct + 16;
  size_t ct_len = len - 16;
  out.resize(ct_len);
  mbedtls_aes_context ctx;
  mbedtls_aes_init(&ctx);
  mbedtls_aes_setkey_dec(&ctx, key, 128);
  std::vector<uint8_t> buf(ct_len);
  memcpy(buf.data(), ct, ct_len);
  uint8_t ivbuf[16];
  memcpy(ivbuf, iv, 16);
  mbedtls_aes_crypt_cbc(&ctx, MBEDTLS_AES_DECRYPT, ct_len, ivbuf, buf.data(), out.data());
  mbedtls_aes_free(&ctx);
  if (!out.empty()) {
    uint8_t pad = out.back();
    if (pad>0 && pad<=16 && out.size()>=pad) {
      out.resize(out.size()-pad);
    }
  }
}

bool WiFiCredentialManager::isRateLimited() {
  unsigned long now = millis();
  if (lastCommitWindowStart==0 || (now - lastCommitWindowStart) > 60000) {
    lastCommitWindowStart = now;
    commitAttempts = 0;
  }
  if (commitAttempts>=5) return true;
  return false;
}
