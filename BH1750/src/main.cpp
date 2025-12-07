#include <Arduino.h>
#include <Wire.h>
#include <BH1750.h>

BH1750 lightMeter;

// Calibration variables - YOU WILL UPDATE THESE
float calibrationGain = 1.0;    // Start with no gain correction
float calibrationOffset = 0.0;  // Start with no offset

// Data collection variables
const int numReadings = 5;      // Number of samples to average
int readingCount = 0;
const int maxDataPoints = 20;   // Maximum calibration data points

struct CalibrationData {
  float bh1750Raw;
  float ut383Reference;
};

CalibrationData dataPoints[maxDataPoints];
int currentDataPoint = 0;
bool calibrationComplete = false;

// ADDR pin configuration
const int addrPin = A3;

// Function declarations
void handleCommand(char command, float currentRawLux);
void collectCalibrationPoint(float currentRawLux);
void showCollectedData();
void calculateCalibrationFactors();
void showCalibrationAccuracy();
void resetCalibrationData();
void showCurrentMeasurement(float currentRawLux);
void showDebugInfo();

void setup() {
  Serial.begin(115200);
  
  // Initialize I2C with your specific pinout
  Wire.begin(); // SDA = A4, SCL = A5 (default for Arduino Uno)
  
  // Configure ADDR pin
  pinMode(addrPin, INPUT_PULLUP);
  
  // Check ADDR pin state to determine I2C address
  bool addrState = digitalRead(addrPin);
  Serial.print("ADDR pin (A3) state: ");
  Serial.println(addrState ? "HIGH" : "LOW");
  
  // Try to initialize BH1750 with appropriate address
  bool sensorFound = false;
  
  if (addrState == LOW) {
    // ADDR connected to GND - use address 0x23
    Serial.println("ADDR pin is LOW, using I2C address 0x23");
    sensorFound = lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, 0x23);
  } else {
    // ADDR connected to VCC or floating - use address 0x5C
    Serial.println("ADDR pin is HIGH, using I2C address 0x5C");
    sensorFound = lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, 0x5C);
  }
  
  if (!sensorFound) {
    Serial.println("Error: Could not find BH1750 sensor!");
    Serial.println("Please check wiring:");
    Serial.println("  VCC -> 3.3V or 5V");
    Serial.println("  GND -> GND");
    Serial.println("  SCL -> A5");
    Serial.println("  SDA -> A4");
    Serial.println("  ADDR -> A3 (connect to GND for 0x23, VCC for 0x5C)");
    
    // Try alternative address as fallback
    Serial.println("Trying alternative address...");
    if (addrState == LOW) {
      sensorFound = lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, 0x5C);
    } else {
      sensorFound = lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, 0x23);
    }
    
    if (sensorFound) {
      Serial.println("Sensor found at alternative address!");
    } else {
      Serial.println("Sensor not found at any address. Stopping.");
      while (1) { 
        delay(1000);
        Serial.println("Please check wiring and reset...");
      }
    }
  } else {
    Serial.println("BH1750 sensor initialized successfully!");
  }
  
  Serial.println(F("BH1750 Calibration Program"));
  Serial.println(F("============================"));
  Serial.println(F("Pin Configuration:"));
  Serial.println(F("  SCL -> A5"));
  Serial.println(F("  SDA -> A4"));
  Serial.println(F("  ADDR -> A3"));
  Serial.println(F("Commands:"));
  Serial.println(F("  'c' - Start new calibration measurement"));
  Serial.println(F("  's' - Show collected data points"));
  Serial.println(F("  'f' - Finish and calculate calibration factors"));
  Serial.println(F("  'r' - Reset collected data"));
  Serial.println(F("  'm' - Show current calibrated measurement"));
  Serial.println(F("  'd' - Debug sensor info"));
  Serial.println(F("============================\n"));
}

void loop() {
  // Read sensor continuously
  float rawLux = lightMeter.readLightLevel();
  
  // Display automatic readings every 2 seconds
  static unsigned long lastDisplay = 0;
  if (millis() - lastDisplay >= 2000) {
    lastDisplay = millis();
    float calibratedLux = (rawLux * calibrationGain) + calibrationOffset;
    Serial.print("Auto Reading - Raw: ");
    Serial.print(rawLux);
    Serial.print(" lux");
    if (calibrationComplete) {
      Serial.print(" | Calibrated: ");
      Serial.print(calibratedLux);
      Serial.print(" lux");
    }
    Serial.println();
  }
  
  // Handle serial commands
  if (Serial.available()) {
    char command = Serial.read();
    handleCommand(command, rawLux);
  }
  
  delay(500); // Shorter delay for more responsive commands
}

void handleCommand(char command, float currentRawLux) {
  switch (command) {
    case 'c': // Collect calibration point
      collectCalibrationPoint(currentRawLux);
      break;
      
    case 's': // Show collected data
      showCollectedData();
      break;
      
    case 'f': // Finish calibration
      calculateCalibrationFactors();
      break;
      
    case 'r': // Reset data
      resetCalibrationData();
      break;
      
    case 'm': // Show current measurement
      showCurrentMeasurement(currentRawLux);
      break;
      
    case 'd': // Debug info
      showDebugInfo();
      break;
      
    default:
      Serial.println("Unknown command. Use: c, s, f, r, m, d");
      break;
  }
}

void showDebugInfo() {
  Serial.println("\n=== Debug Information ===");
  Serial.println("Pin Configuration:");
  Serial.println("  SCL: A5");
  Serial.println("  SDA: A4"); 
  Serial.println("  ADDR: A3");
  
  // Check current ADDR pin state
  bool addrState = digitalRead(addrPin);
  Serial.print("ADDR pin current state: ");
  Serial.println(addrState ? "HIGH (0x5C)" : "LOW (0x23)");
  
  // Scan I2C bus
  Serial.println("Scanning I2C bus...");
  byte error, address;
  int nDevices = 0;
  for(address = 1; address < 127; address++ ) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
    if (error == 0) {
      Serial.print("I2C device found at address 0x");
      if (address < 16) Serial.print("0");
      Serial.println(address, HEX);
      nDevices++;
    }
  }
  if (nDevices == 0) {
    Serial.println("No I2C devices found!");
  } else {
    Serial.println("I2C scan complete.");
  }
}

void collectCalibrationPoint(float currentRawLux) {
  if (currentDataPoint >= maxDataPoints) {
    Serial.println("Error: Maximum data points reached! Use 'f' to finish or 'r' to reset.");
    return;
  }
  
  Serial.println("\n=== Collecting Calibration Point ===");
  Serial.print("Current BH1750 Raw Reading: ");
  Serial.print(currentRawLux);
  Serial.println(" lux");
  Serial.println("Please enter the reference value from UT383 meter:");
  
  // Wait for reference value input
  while (!Serial.available()) {
    delay(100);
  }
  
  String input = Serial.readStringUntil('\n');
  float referenceValue = input.toFloat();
  
  if (referenceValue > 0) {
    dataPoints[currentDataPoint].bh1750Raw = currentRawLux;
    dataPoints[currentDataPoint].ut383Reference = referenceValue;
    
    Serial.print("Stored: BH1750=");
    Serial.print(currentRawLux);
    Serial.print(" lux, UT383=");
    Serial.print(referenceValue);
    Serial.println(" lux");
    
    currentDataPoint++;
    Serial.print("Data points collected: ");
    Serial.println(currentDataPoint);
  } else {
    Serial.println("Error: Invalid input! Please enter a positive number.");
  }
}

void showCollectedData() {
  Serial.println("\n=== Collected Calibration Data ===");
  Serial.println("Point#\tBH1750 Raw\tUT383 Reference");
  Serial.println("-----------------------------------");
  
  for (int i = 0; i < currentDataPoint; i++) {
    Serial.print(i + 1);
    Serial.print("\t");
    Serial.print(dataPoints[i].bh1750Raw);
    Serial.print(" lux\t");
    Serial.print(dataPoints[i].ut383Reference);
    Serial.println(" lux");
  }
  
  if (currentDataPoint == 0) {
    Serial.println("No data collected yet. Use 'c' to collect points.");
  }
}

void calculateCalibrationFactors() {
  if (currentDataPoint < 2) {
    Serial.println("Error: Need at least 2 data points for calibration!");
    return;
  }
  
  Serial.println("\n=== Calculating Calibration Factors ===");
  
  // Simple linear regression: y = mx + b
  // where y = UT383 reference, x = BH1750 raw
  float sumX = 0, sumY = 0, sumXY = 0, sumX2 = 0;
  int n = currentDataPoint;
  
  for (int i = 0; i < n; i++) {
    float x = dataPoints[i].bh1750Raw;
    float y = dataPoints[i].ut383Reference;
    
    sumX += x;
    sumY += y;
    sumXY += x * y;
    sumX2 += x * x;
  }
  
  // Calculate slope (gain) and intercept (offset)
  float denominator = (n * sumX2 - sumX * sumX);
  
  if (denominator != 0) {
    calibrationGain = (n * sumXY - sumX * sumY) / denominator;
    calibrationOffset = (sumY - calibrationGain * sumX) / n;
    
    Serial.println("Calibration Complete!");
    Serial.print("Calibration Formula: Corrected_Lux = (Raw * ");
    Serial.print(calibrationGain, 6);
    Serial.print(") + ");
    Serial.println(calibrationOffset, 6);
    Serial.println("\nUse these values in your final code:");
    Serial.print("float calibrationGain = ");
    Serial.print(calibrationGain, 6);
    Serial.println(";");
    Serial.print("float calibrationOffset = ");
    Serial.print(calibrationOffset, 6);
    Serial.println(";");
    
    calibrationComplete = true;
    
    // Show calibration accuracy
    showCalibrationAccuracy();
    
  } else {
    Serial.println("Error: Cannot calculate calibration factors (division by zero)");
  }
}

void showCalibrationAccuracy() {
  Serial.println("\n=== Calibration Accuracy ===");
  Serial.println("Point#\tRaw\tReference\tCorrected\tError");
  Serial.println("------------------------------------------------");
  
  for (int i = 0; i < currentDataPoint; i++) {
    float raw = dataPoints[i].bh1750Raw;
    float reference = dataPoints[i].ut383Reference;
    float corrected = (raw * calibrationGain) + calibrationOffset;
    float error = abs(corrected - reference);
    float errorPercent = (error / reference) * 100;
    
    Serial.print(i + 1);
    Serial.print("\t");
    Serial.print(raw, 1);
    Serial.print("\t");
    Serial.print(reference, 1);
    Serial.print("\t");
    Serial.print(corrected, 1);
    Serial.print("\t");
    Serial.print(error, 1);
    Serial.print(" lux (");
    Serial.print(errorPercent, 1);
    Serial.println("%)");
  }
}

void resetCalibrationData() {
  currentDataPoint = 0;
  calibrationComplete = false;
  calibrationGain = 1.0;
  calibrationOffset = 0.0;
  Serial.println("Calibration data reset. Ready for new calibration.");
}

void showCurrentMeasurement(float currentRawLux) {
  float calibratedLux = (currentRawLux * calibrationGain) + calibrationOffset;
  
  Serial.println("\n=== Current Measurement ===");
  Serial.print("BH1750 Raw: ");
  Serial.print(currentRawLux);
  Serial.println(" lux");
  
  if (calibrationComplete) {
    Serial.print("Calibrated: ");
    Serial.print(calibratedLux);
    Serial.println(" lux");
    Serial.print("(Using: gain=");
    Serial.print(calibrationGain, 6);
    Serial.print(", offset=");
    Serial.print(calibrationOffset, 6);
    Serial.println(")");
  } else {
    Serial.println("No calibration applied (using default values)");
  }
}