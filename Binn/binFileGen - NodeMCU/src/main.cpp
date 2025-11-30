#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>

#if defined(ESP8266)
#define SCK_PIN   14
#define MISO_PIN  12
#define MOSI_PIN  13
#define SS_PIN    15
#define RST_PIN   16
#define DIO0_PIN  5
#define LED_PIN   LED_BUILTIN
#elif defined(ESP32)
#define SCK_PIN   18
#define MISO_PIN  19
#define MOSI_PIN  23
#define SS_PIN    27
#define RST_PIN   -1
#define DIO0_PIN  -1
#define LED_PIN   2
#else
#define SCK_PIN   14
#define MISO_PIN  12
#define MOSI_PIN  13
#define SS_PIN    15
#define RST_PIN   16
#define DIO0_PIN  5
#define LED_PIN   LED_BUILTIN
#endif

#define FREQUENCY 433E6

int counter = 0;
unsigned long lastSend = 0;
bool loraReady = false;
unsigned long lastBlink = 0;
bool ledOn = false;
const unsigned long blinkIntervalMs = 500;
unsigned long lastRetry = 0;
bool settingsPrinted = false;

static void configureLoRa() {
  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);
  LoRa.setSyncWord(0x12);
  LoRa.enableCrc();
}

static uint8_t readLoRaVersion() {
  pinMode(SS_PIN, OUTPUT);
  digitalWrite(SS_PIN, HIGH);
  digitalWrite(SS_PIN, LOW);
  SPI.transfer(0x42 & 0x7F);
  uint8_t v = SPI.transfer(0x00);
  digitalWrite(SS_PIN, HIGH);
  return v;
}

static void printSettings() {
  Serial.println();
  Serial.println(F("Settings:"));
  Serial.println(F("  Frequency: 433 MHz"));
  Serial.println(F("  Spreading Factor: 7"));
  Serial.println(F("  Bandwidth: 125 kHz"));
  Serial.println(F("  Coding Rate: 4/5"));
  Serial.println();
  Serial.println(F("--- Ready to TX/RX ---"));
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  SPI.begin(
#if defined(ESP32)
    SCK_PIN, MISO_PIN, MOSI_PIN, SS_PIN
#endif
  );

  LoRa.setPins(SS_PIN, RST_PIN, DIO0_PIN);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  Serial.print(F("Initializing LoRa at "));
  Serial.print(FREQUENCY / 1E6);
  Serial.println(F(" MHz..."));
  uint8_t ver = readLoRaVersion();
  Serial.print(F("SX127x version reg: 0x")); Serial.println(ver, HEX);

  if (LoRa.begin(FREQUENCY)) {
    loraReady = true;
    configureLoRa();
    digitalWrite(LED_PIN, HIGH);
  } else {
    digitalWrite(LED_PIN, LOW);
  }
}

void loop() {
  if (!loraReady) {
    if (millis() - lastRetry > 1000) {
      lastRetry = millis();
      Serial.println(F("Retrying LoRa init..."));
      if (LoRa.begin(FREQUENCY)) {
        loraReady = true;
        configureLoRa();
        lastBlink = 0;
        digitalWrite(LED_PIN, HIGH);
        settingsPrinted = false;
      } else {
        digitalWrite(LED_PIN, LOW);
      }
    }
  } else {
    if (!settingsPrinted) {
      printSettings();
      settingsPrinted = true;
    }
    if (millis() - lastBlink > blinkIntervalMs) {
      lastBlink = millis();
      ledOn = !ledOn;
      digitalWrite(LED_PIN, ledOn ? LOW : HIGH);
    }
    if (millis() - lastSend > 2000) {
      lastSend = millis();
      Serial.print(F("TX #"));
      Serial.print(counter);
      Serial.print(F("... "));
      LoRa.beginPacket();
      LoRa.print("ESP8266 Test #");
      LoRa.print(counter);
      if (LoRa.endPacket()) {
        Serial.println(F("✓ Sent"));
      } else {
        Serial.println(F("✗ Failed"));
      }
      counter++;
    }
    int packetSize = LoRa.parsePacket();
    if (packetSize) {
      Serial.println();
      Serial.println(F("--- Received Packet ---"));
      Serial.print(F("Size: "));
      Serial.print(packetSize);
      Serial.println(F(" bytes"));
      Serial.print(F("Data: "));
      while (LoRa.available()) {
        Serial.print((char)LoRa.read());
      }
      Serial.println();
      Serial.print(F("RSSI: "));
      Serial.print(LoRa.packetRssi());
      Serial.println(F(" dBm"));
      Serial.print(F("SNR: "));
      Serial.println(LoRa.packetSnr());
      Serial.print(F("Freq Error: "));
      Serial.print(LoRa.packetFrequencyError());
      Serial.println(F(" Hz"));
      Serial.println(F("-----------------------"));
      Serial.println();
    }
  }
#if defined(ESP8266)
  yield();
#endif
}