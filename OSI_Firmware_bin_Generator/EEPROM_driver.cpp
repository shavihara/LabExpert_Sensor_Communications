#include <Arduino.h>
#include <Wire.h>
#include <string.h>

// AT24C02 Configuration
const int I2C_SDA = 18;
const int I2C_SCL = 19;
const uint8_t EEPROM_ADDR = 0x50; // Base address for AT24C02 (A0=A1=A2=GND)

// EEPROM Specifications for AT24C02
const uint16_t EEPROM_SIZE = 256; // 256 bytes total
const uint8_t PAGE_SIZE = 8;      // 8-byte page write

// Forward declarations
void scanI2C();
void readEEPROM();
void writeTestData();
void eraseEEPROM();
void runTest();
void showHelp();

class AT24C02
{
private:
    uint8_t deviceAddr;

public:
    AT24C02(uint8_t addr = EEPROM_ADDR) : deviceAddr(addr) {}

    // Initialize I2C
    void begin()
    {
        Wire.begin(I2C_SDA, I2C_SCL);
        Wire.setClock(100000); // 100kHz for EEPROM
    }

    // Write single byte to EEPROM
    bool writeByte(uint16_t memoryAddr, uint8_t data)
    {
        if (memoryAddr >= EEPROM_SIZE)
            return false;

        Wire.beginTransmission(deviceAddr);
        Wire.write(memoryAddr & 0xFF); // Only low byte for AT24C02
        Wire.write(data);

        if (Wire.endTransmission() == 0)
        {
            delay(5); // Wait for write cycle (max 5ms)
            return true;
        }
        return false;
    }

    // Read single byte from EEPROM
    uint8_t readByte(uint16_t memoryAddr)
    {
        if (memoryAddr >= EEPROM_SIZE)
            return 0xFF;

        Wire.beginTransmission(deviceAddr);
        Wire.write(memoryAddr & 0xFF); // Only low byte for AT24C02
        Wire.endTransmission();

        Wire.requestFrom(deviceAddr, (uint8_t)1);
        if (Wire.available())
        {
            return Wire.read();
        }
        return 0xFF;
    }

    // Write multiple bytes (page write)
    bool writePage(uint16_t memoryAddr, uint8_t *data, uint8_t length)
    {
        if (memoryAddr >= EEPROM_SIZE || length == 0)
            return false;

        // Check page boundary
        uint8_t pageOffset = memoryAddr % PAGE_SIZE;
        if (pageOffset + length > PAGE_SIZE)
        {
            length = PAGE_SIZE - pageOffset;
        }

        Wire.beginTransmission(deviceAddr);
        Wire.write(memoryAddr & 0xFF); // Only low byte for AT24C02

        for (uint8_t i = 0; i < length; i++)
        {
            Wire.write(data[i]);
        }

        if (Wire.endTransmission() == 0)
        {
            delay(5); // Wait for write cycle
            return true;
        }
        return false;
    }

    // Read multiple bytes
    bool readBytes(uint16_t memoryAddr, uint8_t *buffer, uint8_t length)
    {
        if (memoryAddr >= EEPROM_SIZE || length == 0)
            return false;

        Wire.beginTransmission(deviceAddr);
        Wire.write(memoryAddr & 0xFF); // Only low byte for AT24C02
        Wire.endTransmission();

        Wire.requestFrom(deviceAddr, length);
        for (uint8_t i = 0; i < length && Wire.available(); i++)
        {
            buffer[i] = Wire.read();
        }
        return true;
    }

    // Erase entire EEPROM (fill with 0xFF)
    bool eraseAll()
    {
        uint8_t emptyPage[PAGE_SIZE];
        memset(emptyPage, 0xFF, PAGE_SIZE);

        for (uint16_t addr = 0; addr < EEPROM_SIZE; addr += PAGE_SIZE)
        {
            if (!writePage(addr, emptyPage, PAGE_SIZE))
            {
                return false;
            }
        }
        return true;
    }

    // Erase specific range
    bool eraseRange(uint16_t startAddr, uint16_t endAddr)
    {
        if (startAddr >= EEPROM_SIZE || endAddr >= EEPROM_SIZE || startAddr > endAddr)
        {
            return false;
        }

        for (uint16_t addr = startAddr; addr <= endAddr; addr++)
        {
            if (!writeByte(addr, 0xFF))
            {
                return false;
            }
        }
        return true;
    }

    // Check if EEPROM is responsive
    bool isConnected()
    {
        Wire.beginTransmission(deviceAddr);
        return (Wire.endTransmission() == 0);
    }

    // Get EEPROM size
    uint16_t getSize()
    {
        return EEPROM_SIZE;
    }
};

// Create EEPROM object
AT24C02 eeprom;

void setup()
{
    Serial.begin(115200);
    eeprom.begin();

    Serial.println();
    Serial.println("=== AT24C02 EEPROM Demo ===");
    Serial.println("Commands: r=read, w=write, e=erase, s=scan, t=test");
    Serial.println("============================");

    // Check if EEPROM is connected
    if (!eeprom.isConnected())
    {
        Serial.println("ERROR: EEPROM not found! Check wiring:");
        Serial.println("  SDA -> GPIO 21");
        Serial.println("  SCL -> GPIO 22");
        Serial.println("  VCC -> 3.3V");
        Serial.println("  GND -> GND");
        Serial.println("  A0,A1,A2,WP -> GND");
        while (1)
            delay(1000);
    }

    Serial.println("✓ EEPROM connected successfully");
    Serial.print("EEPROM Size: ");
    Serial.print(eeprom.getSize());
    Serial.println(" bytes");

    delay(1000);
}

void loop()
{
    if (Serial.available())
    {
        char command = Serial.read();

        switch (command)
        {
        case 's': // Scan I2C
            scanI2C();
            break;
        case 'r': // Read EEPROM
            readEEPROM();
            break;
        case 'w': // Write test data
            writeTestData();
            break;
        case 'e': // Erase EEPROM
            eraseEEPROM();
            break;
        case 't': // Run comprehensive test
            runTest();
            break;
        case '?': // Help
            showHelp();
            break;
        default:
            if (command != '\n' && command != '\r')
            {
                Serial.println("Unknown command. Type '?' for help.");
            }
        }
    }

    delay(100);
}

void scanI2C()
{
    Serial.println("\nScanning I2C bus...");
    byte error, address;
    int found = 0;

    for (address = 1; address < 127; address++)
    {
        Wire.beginTransmission(address);
        error = Wire.endTransmission();

        if (error == 0)
        {
            Serial.print("Found device at 0x");
            if (address < 16)
                Serial.print("0");
            Serial.print(address, HEX);

            if (address >= 0x50 && address <= 0x57)
            {
                Serial.println(" (EEPROM)");
            }
            else
            {
                Serial.println();
            }
            found++;
        }
    }

    if (found == 0)
    {
        Serial.println("No I2C devices found!");
    }
}

void readEEPROM()
{
    Serial.println("\nReading EEPROM contents (first 64 bytes):");
    Serial.println("Addr: 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F  ASCII");
    Serial.println("------------------------------------------------------------");

    uint8_t buffer[16];

    for (uint16_t base = 0; base < 64; base += 16)
    {
        Serial.print("0x");
        if (base < 16)
            Serial.print("0");
        Serial.print(base, HEX);
        Serial.print(": ");

        // Read 16 bytes
        eeprom.readBytes(base, buffer, 16);

        // Print hex values
        for (int i = 0; i < 16; i++)
        {
            if (buffer[i] < 16)
                Serial.print("0");
            Serial.print(buffer[i], HEX);
            Serial.print(" ");
        }

        Serial.print(" ");

        // Print ASCII values
        for (int i = 0; i < 16; i++)
        {
            if (buffer[i] >= 32 && buffer[i] <= 126)
            {
                Serial.print((char)buffer[i]);
            }
            else
            {
                Serial.print(".");
            }
        }
        Serial.println();
    }
}

void writeTestData()
{
    Serial.println("\nWriting test data to EEPROM...");

    // Write some test data
    char testString[33]; // allow up to 32 chars + null terminator
    Serial.println("Enter a test string (max 32 chars): ");
    // Wait for user input
    while (!Serial.available())
        delay(10);
    int len = Serial.readBytesUntil('\n', testString, 32);
    testString[len] = '\0';

    for (int i = 0; i < len; i++)
    {
        eeprom.writeByte(i, (uint8_t)testString[i]);
    }

    // Write some numbers
    for (int i = 0; i < 16; i++)
    {
        eeprom.writeByte(32 + i, i);
    }

    Serial.println("Test data written successfully!");
    Serial.println("Use 'r' command to read and verify.");
}

void eraseEEPROM()
{
    Serial.println("\nErasing EEPROM...");

    if (eeprom.eraseAll())
    {
        Serial.println("EEPROM erased successfully (all bytes set to 0xFF)");
    }
    else
    {
        Serial.println("ERROR: Failed to erase EEPROM!");
    }
}

void runTest()
{
    Serial.println("\nRunning comprehensive EEPROM test...");

    // Test 1: Single byte write/read
    Serial.print("1. Single byte test... ");
    eeprom.writeByte(0x10, 0xAA);
    if (eeprom.readByte(0x10) == 0xAA)
    {
        Serial.println("PASS");
    }
    else
    {
        Serial.println("FAIL");
    }

    // Test 2: Page write/read
    Serial.print("2. Page write test... ");
    uint8_t writeData[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    eeprom.writePage(0x20, writeData, 8);

    uint8_t readData[8];
    eeprom.readBytes(0x20, readData, 8);

    bool pageTestPass = true;
    for (int i = 0; i < 8; i++)
    {
        if (writeData[i] != readData[i])
        {
            pageTestPass = false;
            break;
        }
    }
    Serial.println(pageTestPass ? "PASS" : "FAIL");

    // Test 3: Boundary test
    Serial.print("3. Boundary test... ");
    eeprom.writeByte(0xFF, 0x55); // Last byte
    if (eeprom.readByte(0xFF) == 0x55)
    {
        Serial.println("PASS");
    }
    else
    {
        Serial.println("FAIL");
    }

    Serial.println("Test completed!");
}

void showHelp()
{
    Serial.println("\nAvailable Commands:");
    Serial.println("s - Scan I2C bus for devices");
    Serial.println("r - Read and display EEPROM contents");
    Serial.println("w - Write test data to EEPROM");
    Serial.println("e - Erase entire EEPROM (fill with 0xFF)");
    Serial.println("t - Run comprehensive read/write test");
    Serial.println("? - Show this help message");
}