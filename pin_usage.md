# LabExpert Sensor Board - Pin Usage Guide

## Summary: All Pin Usage
This table aggregates all pin usage across different experiments/firmwares on the ESP32.

| Pin | Usage (Experiment: Function) | Hardware Configuration |
| :--- | :--- | :--- |
| **13** | **All**: Status LED | Output |
| **14** | **OSI**: WiFi LED, **OTA**: WiFi LED | Output |
| **18** | **All**: EEPROM SDA | External Pullup (I2C) |
| **19** | **All**: EEPROM SCL | External Pullup (I2C) |
| **21** | **TOF**: SDA, **ULT**: Trig, **BH**: SDA, **Mic**: SD | External Pullup (I2C) |
| **22** | **TOF**: SCL, **ULT**: Echo, **BH**: SCL, **Mic**: SCK | External Pullup (I2C) |
| **23** | **THR**: OneWire, **TOF**: LDR, **OTA**: EEPROM WP | Ext Pullup (THR) / Int Pullup (TOF) / Output (OTA) |
| **26** | **TOF**: Motor Left PWM, **Mic**: WS | Output |
| **32** | **OSI**: Restart Trigger | Internal Pullup |
| **33** | **TOF**: Motor Right PWM, **OSI**: Sensor | Output (TOF) / Internal Pullup (OSI) |

---

## 1. THR Experiment (Temperature - DS18B20)
*Project: `THR_Firmware_bin_Generator`*

| Pin | Name | Usage | Configuration |
| :--- | :--- | :--- | :--- |
| **23** | `ONE_WIRE_BUS` | **DS18B20 Data** | Input/Output (Requires External Pullup ~4.7kΩ) |
| **18** | `EEPROM_SDA` | **EEPROM I2C Data** | I2C (Requires Pullup) |
| **19** | `EEPROM_SCL` | **EEPROM I2C Clock**| I2C (Requires Pullup) |
| **13** | `STATUS_LED` | **Status LED** | Output |

---

## 2. TOF Experiment (Time of Flight - VL53L1X + Motor)
*Project: `TOF_Firmware_bin_Generator`*

| Pin | Name | Usage | Configuration |
| :--- | :--- | :--- | :--- |
| **21** | `TOF_SDA` | **VL53L1X Data** | I2C (Requires Pullup) |
| **22** | `TOF_SCL` | **VL53L1X Clock** | I2C (Requires Pullup) |
| **33** | `RPWM_PIN` | **Motor Right PWM** | Output |
| **26** | `LPWM_PIN` | **Motor Left PWM** | Output |
| **23** | `LDR_PIN` | **Encoder/Limit Switch**| **Input Internal Pullup** (`INPUT_PULLUP`) |
| **18** | `EEPROM_SDA` | **EEPROM I2C Data** | I2C (Requires Pullup) |
| **19** | `EEPROM_SCL` | **EEPROM I2C Clock**| I2C (Requires Pullup) |
| **13** | `SENSOR_LED` | **Status LED** | Output |

---

## 3. OSI Experiment (Oscillation Sensor)
*Project: `OSI_Firmware_bin_Generator`*

| Pin | Name | Usage | Configuration |
| :--- | :--- | :--- | :--- |
| **33** | `SENSOR_PIN` | **Oscillation Sensor** | **Input** (Firmware default. Recommend External Pullup if sensor is Open-Drain/Collector, or change FW to `INPUT_PULLUP`) |
| **32** | `RESTART_PIN`| **OTA Restart Trigger** | **Input Internal Pullup** (`INPUT_PULLUP`) |
| **18** | `EEPROM_SDA` | **EEPROM I2C Data** | I2C (Requires Pullup) |
| **19** | `EEPROM_SCL` | **EEPROM I2C Clock**| I2C (Requires Pullup) |
| **14** | `WIFI_LED` | **WiFi Status LED** | Output |
| **13** | `SENSOR_LED` | **Sensor Status LED** | Output |

---

## 4. UltraSonic Experiment (HC-SR04)
*Project: `UltraSonic_Firmware_bin_Generator`*

| Pin | Name | Usage | Configuration |
| :--- | :--- | :--- | :--- |
| **21** | `TRIG_PIN` | **Sensor Trigger** | Output (Strong driver overrides I2C pullups) |
| **22** | `ECHO_PIN` | **Sensor Echo** | Input (Sensor drives High/Low, overrides I2C pullups) |
| **18** | `EEPROM_SDA` | **EEPROM I2C Data** | I2C (Requires Pullup) |
| **19** | `EEPROM_SCL` | **EEPROM I2C Clock**| I2C (Requires Pullup) |
| **13** | `SENSOR_LED` | **Status LED** | Output |

---

## 5. Light Intensity Experiment (BH1750)
*Target: ESP32 (Shared Bus w/ TOF/ULT)*

| Pin | Name | Usage | Configuration |
| :--- | :--- | :--- | :--- |
| **21** | `SDA` | **BH1750 Data** | I2C (Requires Pullup - Shared with TOF) |
| **22** | `SCL` | **BH1750 Clock** | I2C (Requires Pullup - Shared with TOF) |
| **18** | `EEPROM_SDA` | **EEPROM I2C Data** | I2C (Standard System Bus) |
| **19** | `EEPROM_SCL` | **EEPROM I2C Clock**| I2C (Standard System Bus) |
| **13** | `STATUS_LED` | **Status LED** | Output (Standard System LED) |

---

## 6. Microphone Experiment (INMP441 - I2S)
*Target: ESP32 (Shared Pins)*

**⚠️ CRITICAL WIRING NOTE:**
*   You **CANNOT** ground the **WS (Word Select)** pin. It is a **Clock Signal** driven by the ESP32.
*   You **SHOULD** ground the **L/R** pin to select the Left Channel.

| Pin | Name | Usage | Configuration |
| :--- | :--- | :--- | :--- |
| **22** | `I2S_SCK (BCLK)`| **Serial Clock** | Output (ESP32 drives this) |
| **21** | `I2S_SD (DOUT)` | **Serial Data** | Input (Microphone drives this) |
| **26** | `I2S_WS (LRCLK)`| **Word Select** | Output (ESP32 drives this) - **MUST CONNECT** |
| **GND** | `L/R` | **Channel Select** | **Connect to GND** (Selects Left Channel) |
| **3.3V**| `VDD` | **Power** | |
| **GND** | `GND` | **Ground** | |

---

## 7. OTA Bootloader
*Project: `ESP_32_OTA`*

This is the base firmware that manages updates and sensor detection.

| Pin | Name | Usage | Configuration |
| :--- | :--- | :--- | :--- |
| **14** | `WIFI_LED` | **WiFi Status LED** | Output |
| **12** | `BLE_LED` | **BLE Status LED** | Output |
| **13** | `SENSOR_LED` | **Sensor Status LED** | Output |
| **25** | `EEPROM_WP` | **EEPROM Write Protect**| Output (High = Protect, Low = Write) |
| **18** | `EEPROM_SDA` | **EEPROM I2C Data** | I2C (Requires Pullup) |
| **19** | `EEPROM_SCL` | **EEPROM I2C Clock**| I2C (Requires Pullup) |
