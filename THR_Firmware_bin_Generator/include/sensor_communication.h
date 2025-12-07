#ifndef SENSOR_COMMUNICATION_H
#define SENSOR_COMMUNICATION_H

#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// Pin Configuration
#define ONE_WIRE_BUS 23   // User specified pin 23

// External declarations
extern OneWire oneWire;
extern DallasTemperature sensors;
extern DeviceAddress sensorAddress;

// EEPROM configuration (if we use I2C EEPROM too? The user said "use above code in relative paht as reference" which implies OneWire, but TOF uses I2C EEPROM for ID storage. "still use the MQTT method" implies we need ID. User didn't specify I2C EEPROM but TOF relies on it for `detectSensorFromEEPROM` and `getDeviceIDFromMAC`. 
// Wait, TOF: `EEPROM_SDA 18`, `EEPROM_SCL 19` for I2C EEPROM. `TOF_SDA 21` etc. for sensor.
// THR: Using DS18B20 on pin 23.
// Does THR board also have the I2C EEPROM for ID? The project name is "...Firmware_bin_Generator" likely for the same "LabExpert" ecosystem which uses EEPROM for sensor ID. 
// I should probably keep I2C EEPROM support for ID/SensorType detection if the hardware supports it. 
// User said "use pin 23 on esp for pin2 in arduino".
// User also said "add follow code as core data collecting method ... note: following code is for arduino board ... we are creating for ESP32".
// I will assume I2C EEPROM is present for ID/MAC logic (same base board?). 
// I'll keep the I2C EEPROM detection logic but change sensor logic to DS18B20.
// If not present, `detectSensorFromEEPROM` will fail and trigger failsafe.
// The user says "still use the MQTT method", which uses `sensorID` from `sensor_communication.h`.
// I will definitely include I2C logic for EEPROM too.

// I2C Pins for EEPROM (Same as TOF)
#define EEPROM_SDA 18
#define EEPROM_SCL 19
#define EEPROM_SENSOR_ADDR 0x50
#define EEPROM_SIZE 3
#define EEPROM_RETRY_COUNT 3
#define EEPROM_RETRY_DELAY 1000

// Function declarations
bool initializeTHRSensor();
bool detectSensorFromEEPROM();
String getDeviceIDFromMAC();
void setSensorResolution(int resolution);

// External variables
extern String sensorType;
extern String sensorID;

#endif
