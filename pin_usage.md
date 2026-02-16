# LabExpert Sensor Board - Pin Usage Guide

## Summary: All Pin Usage
This table aggregates all pin usage across different experiments/firmwares on the ESP32.

| Pin | Usage (Experiment: Function) | Hardware Configuration |
| :--- | :--- | :--- |
| **12** | **All**: BLE Status LED | Output (Active Low) |
| **13** | **All**: Sensor Connected LED | Output (Active Low) |
| **14** | **All**: WiFi Status LED | Output (Active Low) |
| **16** | **All**: OTA Status LED | Output (Active Low) |
| **18** | **All**: EEPROM SDA | External Pullup (I2C) |
| **19** | **All**: EEPROM SCL | External Pullup (I2C) |
| **21** | **TOF**: SDA, **ULT**: Trig, **BH**: SDA, **Mic**: SD | External Pullup (I2C) |
| **22** | **TOF**: SCL, **ULT**: Echo, **BH**: SCL, **Mic**: SCK | External Pullup (I2C) |
| **23** | **THR**: OneWire, **TOF**: LDR | Ext Pullup (THR) / Ext Pullup (TOF) |
| **25** | **OTA**: EEPROM WP | Output (High = Protect, Low = Write) |
| **26** | **TOF**: Motor Left PWM, **Mic**: WS | Output |
| **27** | **All**: Sensor Active LED | Output (Active Low) |
| **32** | **OSI**: Restart Trigger | Internal Pullup |
| **33** | **TOF**: Motor Right PWM, **OSI**: Sensor | Output (TOF) / Internal Pullup (OSI) |
| **34** | **OTA**: BLE Trigger / Factory Reset | Input (External 10k Pull-up Required) |
| **35** | **TOF**: Shared Limit Switch (OR Gate) | Input (3.3V Logic - External Supply) |

**LED Logic (Active Low - VCC -> LED -> Pin):**

*   **Pin 14 (WiFi LED):**
    *   **Disconnected:** ON (`LOW`)
    *   **Connected:** Blink (`Toggle`)
    *   **Bluetooth On:** OFF (`HIGH`)
*   **Pin 12 (BLE LED):**
    *   **BLE On (Advertising):** ON (`LOW`)
    *   **BLE Connected:** Blink (`Toggle`)
    *   **BLE Off:** OFF (`HIGH`)
*   **Pin 13 (Sensor Connected LED):**
    *   **Sensor Connected:** OFF (`HIGH`)
    *   **Sensor Disconnected:** ON (`LOW`)
*   **Pin 27 (Sensor Active LED):**
    *   **Sensor Connected & Active:** Blink (`Toggle`)
    *   **Sensor Disconnected/Idle:** OFF (`HIGH`)
*   **Pin 16 (OTA LED):**
    *   **OTA Restart Triggered:** Fast Blink (`Toggle`)

---

## 1. THR Experiment (Temperature - DS18B20)
*Project: `THR_Firmware_bin_Generator`*

| Pin | Name | Usage | Configuration |
| :--- | :--- | :--- | :--- |
| **23** | `ONE_WIRE_BUS` | **DS18B20 Data** | Input/Output (Requires External Pullup ~4.7kΩ) |
| **18** | `EEPROM_SDA` | **EEPROM I2C Data** | I2C (Requires Pullup) |
| **19** | `EEPROM_SCL` | **EEPROM I2C Clock**| I2C (Requires Pullup) |
| **32** | `RESTART_PIN`| **OTA Restart Trigger** | Input Internal Pullup |
| **14** | `WIFI_LED` | **WiFi Status** | Output (Active Low) |
| **12** | `BLE_LED` | **BLE Status** | Output (Active Low) |
| **13** | `SENSOR_LED` | **Sensor Connection** | Output (Active Low) |
| **27** | `ACTIVE_LED` | **Experiment Active** | Output (Active Low) |
| **16** | `OTA_LED` | **OTA Status** | Output (Active Low) |

---

## 2. TOF Experiment (Time of Flight - VL53L1X + Motor)
*Project: `TOF_Firmware_bin_Generator`*

| Pin | Name | Usage | Configuration |
| :--- | :--- | :--- | :--- |
| **21** | `TOF_SDA` | **VL53L1X Data** | I2C (Requires Pullup) |
| **22** | `TOF_SCL` | **VL53L1X Clock** | I2C (Requires Pullup) |
| **33** | `RPWM_PIN` | **Motor Right PWM** | Output |
| **26** | `LPWM_PIN` | **Motor Left PWM** | Output |
| **23** | `LDR_PIN` | **Encoder/Limit Switch**| **Input** (External Pullup exists) |
| **18** | `EEPROM_SDA` | **EEPROM I2C Data** | I2C (Requires Pullup) |
| **19** | `EEPROM_SCL` | **EEPROM I2C Clock**| I2C (Requires Pullup) |
| **32** | `RESTART_PIN`| **OTA Restart Trigger** | Input Internal Pullup |
| **35** | `LIMIT_PIN` | **Combined Limit Switch** | **Input** (Active HIGH. 3.3V Logic) |
| **14, 12, 13, 27, 16** | **LEDs** | **Status Indicators** | Output (Active Low) |

---

## 3. OSI Experiment (Oscillation Sensor)
*Project: `OSI_Firmware_bin_Generator`*

| Pin | Name | Usage | Configuration |
| :--- | :--- | :--- | :--- |
| **33** | `SENSOR_PIN` | **Oscillation Sensor** | **Input** (Internal Pullup recommended) |
| **32** | `RESTART_PIN`| **OTA Restart Trigger** | **Input Internal Pullup** (`INPUT_PULLUP`) |
| **18** | `EEPROM_SDA` | **EEPROM I2C Data** | I2C (Requires Pullup) |
| **19** | `EEPROM_SCL` | **EEPROM I2C Clock**| I2C (Requires Pullup) |
| **14, 12, 13, 27, 16** | **LEDs** | **Status Indicators** | Output (Active Low) |

---

## 4. UltraSonic Experiment (HC-SR04)
*Project: `UltraSonic_Firmware_bin_Generator`*

| Pin | Name | Usage | Configuration |
| :--- | :--- | :--- | :--- |
| **21** | `TRIG_PIN` | **Sensor Trigger** | Output (Strong driver overrides I2C pullups) |
| **22** | `ECHO_PIN` | **Sensor Echo** | Input (Sensor drives High/Low, overrides I2C pullups) |
| **18** | `EEPROM_SDA` | **EEPROM I2C Data** | I2C (Requires Pullup) |
| **19** | `EEPROM_SCL` | **EEPROM I2C Clock**| I2C (Requires Pullup) |
| **32** | `RESTART_PIN`| **OTA Restart Trigger** | Input Internal Pullup |
| **14, 12, 13, 27, 16** | **LEDs** | **Status Indicators** | Output (Active Low) |

---

## 5. Light Intensity Experiment (BH1750)
*Target: ESP32 (Shared Bus w/ TOF/ULT)*

| Pin | Name | Usage | Configuration |
| :--- | :--- | :--- | :--- |
| **21** | `SDA` | **BH1750 Data** | I2C (Shared with TOF) |
| **22** | `SCL` | **BH1750 Clock** | I2C (Shared with TOF) |
| **18, 19** | `EEPROM` | **System Bus** | I2C |
| **13** | `LED` | **Status** | Output |

---

## 6. Microphone Experiment (INMP441 - I2S)
*Target: ESP32 (Shared Pins)*

**⚠️ CRITICAL WIRING NOTE:**
*   **WS (Pin 26)** MUST be connected to ESP32.
*   **L/R** SHOULD be grounded.

| Pin | Name | Usage | Configuration |
| :--- | :--- | :--- | :--- |
| **22** | `BCLK` | **Serial Clock** | Output |
| **21** | `DOUT` | **Serial Data** | Input |
| **26** | `LRCLK`| **Word Select** | Output |

---

## 7. OTA Bootloader
*Project: `ESP_32_OTA`*

| Pin | Name | Usage | Configuration |
| :--- | :--- | :--- | :--- |
| **14** | `WIFI_LED` | **WiFi Status** | Output (Active Low) |
| **12** | `BLE_LED` | **BLE Status** | Output (Active Low) |
| **13** | `SENSOR_LED` | **Sensor Status** | Output (Active Low) |
| **16** | `OTA_LED` | **OTA Status** | Output (Active Low) |
| **27** | `ACTIVE_LED` | **Active Status** | Output (Active Low) |
| **25** | `EEPROM_WP` | **EEPROM Write Protect**| Output (High = Protect, Low = Write) |
| **34** | `BLE_TRIGGER`| **Factory Reset / BLE** | **Input** (Hold 3s to erase creds. Req 10k ext pull-up) |
| **18** | `EEPROM_SDA` | **EEPROM I2C Data** | I2C (Requires Pullup) |
| **19** | `EEPROM_SCL` | **EEPROM I2C Clock**| I2C (Requires Pullup) |
