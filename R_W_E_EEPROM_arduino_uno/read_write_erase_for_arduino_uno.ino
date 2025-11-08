#include <Wire.h>


//SCL(IC PIN 6) = A5
//SDL (IC PIN 5) = A4
// IC PIN 8 = 5v
// IC PIN 1,2,3,4,7 = GND

#define EEPROM_ADDR 0x50  // 24C02 I2C address
#define START_ADDR 0x00
char inputBuffer[32];

void writeEEPROM(uint8_t addr, const char* data) {
  Wire.beginTransmission(EEPROM_ADDR);
  Wire.write(addr);
  while (*data) Wire.write(*data++);
  Wire.endTransmission();
  delay(5);
}

void readEEPROM(uint8_t addr, int len, char* buffer) {
  Wire.beginTransmission(EEPROM_ADDR);
  Wire.write(addr);
  Wire.endTransmission();

  Wire.requestFrom(EEPROM_ADDR, len);
  int i = 0;
  while (Wire.available() && i < len) buffer[i++] = Wire.read();
  buffer[i] = '\0';
}

void eraseEEPROM() {
  for (int i = 0; i < 32; i++) {
    Wire.beginTransmission(EEPROM_ADDR);
    Wire.write(i);
    Wire.write((byte)0x00);
    Wire.endTransmission();
    delay(5);
  }
  Serial.println("✔ EEPROM erased.");
}

void readAllEEPROM() {
  Serial.println("EEPROM dump (0x00 - 0xFF):");
  for (int addr = 0; addr < 256; addr++) {
    Wire.beginTransmission(EEPROM_ADDR);
    Wire.write(addr);
    Wire.endTransmission();

    Wire.requestFrom(EEPROM_ADDR, 1);
    if (Wire.available()) {
      byte b = Wire.read();

      if (addr % 16 == 0) {
        Serial.print("\n");
        if (addr < 16) Serial.print("0");
        Serial.print(addr, HEX);
        Serial.print(": ");
      }

      if (b < 16) Serial.print("0");
      Serial.print(b, HEX);
      Serial.print(" ");
    }
  }
  Serial.println("\n--- Done ---");
}

void printMenu() {
  Serial.println(F("\n=== 24C02 EEPROM TOOL ==="));
  Serial.println(F("[1] Write Sensor ID"));
  Serial.println(F("[2] Read Sensor ID"));
  Serial.println(F("[3] Erase EEPROM"));
  Serial.println(F("[4] Dump All EEPROM"));
  Serial.println(F("Type option number and press Enter"));
}

void setup() {
  Wire.begin();
  Serial.begin(9600);
  delay(500);
  printMenu();
}

void loop() {
  static byte state = 0;
  static int idx = 0;

  while (Serial.available()) {
    char c = Serial.read();

    if (state == 0) {  // Menu selection state
      if (c == '\n' || c == '\r') continue;

      switch (c) {
        case '1':
          Serial.println("Type new ID (max 31 chars), then press Enter:");
          state = 1;
          idx = 0;
          break;
        case '2': {
          char readBuf[32];
          readEEPROM(START_ADDR, 31, readBuf);
          Serial.print("✔ Current ID: ");
          Serial.println(readBuf);
          printMenu();
          break;
        }
        case '3':
          eraseEEPROM();
          printMenu();
          break;
        case '4':
          readAllEEPROM();
          printMenu();
          break;
        default:
          Serial.println("Invalid option.");
          printMenu();
      }
    } else if (state == 1) {  // Writing ID input state
      if (c == '\r' || c == '\n') {
        inputBuffer[idx] = '\0';
        writeEEPROM(START_ADDR, inputBuffer);
        Serial.print("✔ ID written: ");
        Serial.println(inputBuffer);
        state = 0;
        printMenu();
      } else if (idx < sizeof(inputBuffer) - 1) {
        inputBuffer[idx++] = c;
      }
    }
  }
}
