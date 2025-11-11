#include "experiment_manager.h"
#include "sensor_communication.h"
#include "mqtt_handler.h"
#include "config_handler.h"
#include <esp_partition.h>
#include <esp_ota_ops.h>
#include <driver/timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#define STATUS_LED 13
#define WIFI_LED 14
#define SENSOR_LED 13

// Experiment data arrays
float distances[MAX_SAMPLES];
unsigned long timestamps[MAX_SAMPLES];
int sampleCount = 0;

// Experiment state variables
bool experimentRunning = false;
bool dataReady = false;
unsigned long experimentStartTime = 0;
unsigned long lastSampleTime = 0;
int sampleInterval = 1000 / 50;

// Sensor detection variables
unsigned long lastSensorCheck = 0;
const unsigned long SENSOR_CHECK_INTERVAL = 5000;
bool sensorWasPresent = false;
unsigned long lastExperimentEnd = 0;
bool backendCleanupRequested = false;

// Hardware timer and queue
QueueHandle_t sensorDataQueue = NULL;
volatile bool timerInitialized = false;
volatile bool sampleRequested = false;

// Binary data batching
BinarySample sampleBuffer[BINARY_MAX_SAMPLES_PER_PACKET];
uint16_t bufferedSampleCount = 0;

// CRITICAL FIX: Pre-captured timestamps
volatile unsigned long preCapturedTimestamp = 0;
TaskHandle_t sensorTaskHandle = NULL;

// Forward declarations
void flushSampleBuffer();
void sensorReadingTask(void *parameter);

// Timer ISR - captures timestamp FIRST
void IRAM_ATTR timerISR(void *arg)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (experimentRunning)
    {
        // CRITICAL: Capture timestamp IMMEDIATELY
        preCapturedTimestamp = millis();
        sampleRequested = true;

        // Wake sensor task
        if (sensorTaskHandle != NULL)
        {
            vTaskNotifyGiveFromISR(sensorTaskHandle, &xHigherPriorityTaskWoken);
        }
    }

    timer_group_clr_intr_status_in_isr(TIMER_GROUP_0, TIMER_0);
    timer_group_enable_alarm_in_isr(TIMER_GROUP_0, TIMER_0);

    if (xHigherPriorityTaskWoken)
    {
        portYIELD_FROM_ISR();
    }
}

// Dedicated sensor task on Core 0
void sensorReadingTask(void *parameter)
{
    Serial.println("Sensor task started on Core 0");

    // Performance monitoring variables
    static unsigned long lastSampleTime = 0;
    static int missedSamples = 0;
    static int consecutiveMisses = 0;

    while (true)
    {
        // Wait for timer trigger with minimal timeout
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1));

        if (sampleRequested && experimentRunning)
        {
            // Use pre-captured timestamp
            unsigned long timestamp = preCapturedTimestamp;
            sampleRequested = false;

            // Check for sample timing issues
            unsigned long currentTime = millis();
            if (lastSampleTime > 0)
            {
                unsigned long timeSinceLastSample = currentTime - lastSampleTime;
                int expectedInterval = 1000 / config.frequency;

                // Detect missed samples (more than 1.5x expected interval)
                if (timeSinceLastSample > (expectedInterval * 1.5))
                {
                    missedSamples++;
                    consecutiveMisses++;

                    if (consecutiveMisses > 3)
                    {
                        Serial.printf("WARNING: %d consecutive samples missed at %dHz\n",
                                      consecutiveMisses, config.frequency);
                    }
                }
                else
                {
                    consecutiveMisses = 0;
                }
            }
            lastSampleTime = currentTime;

            // Read sensor (safe now, timestamp already captured)
            uint16_t distance_mm = readUltrasonicDistanceCM();

            if (distance_mm != 65535 && sampleCount < MAX_SAMPLES)
            {
                // Store directly (avoid queue overhead for simplicity)
                timestamps[sampleCount] = timestamp - experimentStartTime;
                distances[sampleCount] = distance_mm;

                // Debug first few samples to verify readings
                if (sampleCount < 10)
                {
                    Serial.printf("Sample %d: Raw=%umm, Time=%lums\n",
                                  sampleCount + 1, distance_mm, timestamps[sampleCount]);
                }

                // Add to buffer for MQTT with overflow protection
                if (bufferedSampleCount < BINARY_MAX_SAMPLES_PER_PACKET)
                {
                    sampleBuffer[bufferedSampleCount].timestamp = timestamps[sampleCount];
                    sampleBuffer[bufferedSampleCount].distance = distance_mm;
                    sampleBuffer[bufferedSampleCount].sample_number = sampleCount + 1;
                    bufferedSampleCount++;
                }
                else
                {
                    // Buffer full, force flush to prevent data loss
                    flushSampleBuffer();

                    // Add current sample to fresh buffer
                    sampleBuffer[0].timestamp = timestamps[sampleCount];
                    sampleBuffer[0].distance = distance_mm;
                    sampleBuffer[0].sample_number = sampleCount + 1;
                    bufferedSampleCount = 1;
                }

                sampleCount++;
                digitalWrite(STATUS_LED, !digitalRead(STATUS_LED));

                // Periodic status report
                if (sampleCount % 50 == 0)
                {
                    Serial.printf("Collected %d samples, %d missed\n", sampleCount, missedSamples);
                }
            }
            else if (distance_mm == 65535)
            {
                // Sensor read error
                Serial.println("Sensor read error (65535), skipping sample");
                missedSamples++;
            }
        }

        // Minimal delay to allow other tasks
        vTaskDelay(0);
    }
}
// Process data in main loop
void processSensorDataQueue()
{
    static unsigned long lastFlushTime = 0;
    unsigned long currentFlushInterval = 50; // Default 50ms

    // Adaptive flush interval based on frequency
    if (config.frequency <= 5)
    {
        currentFlushInterval = 200; // 200ms for low frequencies (1-5Hz)
    }
    else if (config.frequency <= 20)
    {
        currentFlushInterval = 100; // 100ms for medium frequencies (10-20Hz)
    }
    else if (config.frequency <= 50)
    {
        currentFlushInterval = 33; // ~30Hz equivalent (30-50Hz)
    }
    else
    {
        currentFlushInterval = 20; // 20ms for high frequencies (>50Hz)
    }

    // Adaptive batch size triggering
    bool shouldFlush = false;

    if (config.frequency <= 5 && bufferedSampleCount >= BATCH_1_5HZ)
    {
        shouldFlush = true;
    }
    else if (config.frequency <= 20 && bufferedSampleCount >= BATCH_10_20HZ)
    {
        shouldFlush = true;
    }
    else if (config.frequency <= 50 && bufferedSampleCount >= BATCH_30_50HZ)
    {
        shouldFlush = true;
    }
    else if (bufferedSampleCount >= BATCH_HIGH_FREQ)
    {
        shouldFlush = true;
    }

    // Time-based flushing (prevent stale data)
    if (bufferedSampleCount > 0 &&
        (shouldFlush || (millis() - lastFlushTime > currentFlushInterval)))
    {
        flushSampleBuffer();
        lastFlushTime = millis();
    }
}

// Main experiment loop
void manageExperimentLoop()
{
    processSensorDataQueue();

    if (experimentRunning)
    {
        unsigned long currentTime = millis();
        unsigned long elapsedTime = currentTime - experimentStartTime;

        if (config.duration > 0 && elapsedTime >= config.duration * 1000)
        {
            experimentRunning = false;
            dataReady = true;
            lastExperimentEnd = millis();

            // Final flush to ensure all data is sent
            flushSampleBuffer();

            // Small delay to ensure MQTT messages are sent
            delay(10);

            Serial.printf("Experiment COMPLETED. Collected %d samples in %lu ms\n",
                          sampleCount, elapsedTime);

            // Calculate and report data transfer success rate
            int expectedSamples = config.frequency * config.duration;
            int successRate = (sampleCount * 100) / expectedSamples;
            Serial.printf("Data transfer success: %d/%d (%d%%) samples\n",
                          sampleCount, expectedSamples, successRate);

            if (mqttConnected)
            {
                String msg = "Completed with " + String(sampleCount) + "/" + String(expectedSamples) +
                             " samples (" + String(successRate) + "%)";
                publishStatus("experiment_completed", msg.c_str());
            }
        }
    }
    else
    {
        digitalWrite(STATUS_LED, LOW);
    }
}

// Initialize hardware timer
bool initHardwareTimer()
{
    if (timerInitialized)
        return true;

    // Create sensor task on Core 0
    xTaskCreatePinnedToCore(
        sensorReadingTask,
        "SensorTask",
        4096,
        NULL,
        configMAX_PRIORITIES - 1,
        &sensorTaskHandle,
        0 // Core 0
    );

    if (sensorTaskHandle == NULL)
    {
        Serial.println("ERROR: Failed to create sensor task");
        return false;
    }

    // Configure timer
    timer_config_t timerConfig = {
        .alarm_en = TIMER_ALARM_EN,
        .counter_en = TIMER_PAUSE,
        .intr_type = TIMER_INTR_LEVEL,
        .counter_dir = TIMER_COUNT_UP,
        .auto_reload = TIMER_AUTORELOAD_EN,
        .divider = 80};

    timer_init(TIMER_GROUP_0, TIMER_0, &timerConfig);

    int intervalMicroseconds = 1000000 / config.frequency;
    timer_set_alarm_value(TIMER_GROUP_0, TIMER_0, intervalMicroseconds);
    timer_enable_intr(TIMER_GROUP_0, TIMER_0);
    timer_isr_register(TIMER_GROUP_0, TIMER_0, timerISR, NULL, ESP_INTR_FLAG_IRAM, NULL);
    timer_start(TIMER_GROUP_0, TIMER_0);

    timerInitialized = true;
    Serial.printf("Timer initialized for %dHz\n", config.frequency);
    return true;
}

// Update timer frequency
void updateTimerFrequency(int frequency)
{
    if (!timerInitialized)
        return;

    timer_pause(TIMER_GROUP_0, TIMER_0);
    int intervalMicroseconds = 1000000 / frequency;
    timer_set_alarm_value(TIMER_GROUP_0, TIMER_0, intervalMicroseconds);
    sampleInterval = 1000 / frequency;
    timer_start(TIMER_GROUP_0, TIMER_0);

    Serial.printf("Timer frequency updated to %dHz\n", frequency);
}

// Flush sample buffer
void flushSampleBuffer()
{
    if (bufferedSampleCount > 0)
    {
        publishBinarySensorData(sampleBuffer, bufferedSampleCount, experimentStartTime, sampleCount);
        bufferedSampleCount = 0;
    }
}

// Check sensor status
void checkSensorStatus()
{
    if (millis() - lastSensorCheck > SENSOR_CHECK_INTERVAL)
    {
        lastSensorCheck = millis();
        bool sensorCurrentlyPresent = detectSensorFromEEPROM();

        if (sensorWasPresent && !sensorCurrentlyPresent)
        {
            Serial.println("❌ Sensor unplugged! Implementing failsafe mechanism...");
            if (mqttConnected)
            {
                publishStatus("sensor_unplugged", "Switching to bootloader");
            }

            // Check current running partition
            const esp_partition_t *running = esp_ota_get_running_partition();
            Serial.printf("Current running partition: %s\n", running->label);

            // If running on ota_1, switch back to ota_0 and erase ota_1
            if (running && strcmp(running->label, "ota_1") == 0)
            {
                Serial.println("Running on ota_1 with sensor unplugged - switching to ota_0...");

                // Set boot partition to ota_0 (ESP_32_OTA bootloader)
                const esp_partition_t *ota_0 = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);
                if (ota_0 != NULL)
                {
                    esp_err_t err = esp_ota_set_boot_partition(ota_0);
                    if (err == ESP_OK)
                    {
                        Serial.println("✅ Boot partition set to ota_0 (ESP_32_OTA)");

                        // Erase current partition (ota_1) to clean up
                        Serial.println("🗑️ Erasing ota_1 partition...");
                        err = esp_partition_erase_range(running, 0, running->size);
                        if (err == ESP_OK)
                        {
                            Serial.println("✅ ota_1 partition erased successfully");
                        }
                        else
                        {
                            Serial.printf("❌ Failed to erase ota_1 partition: %s\n", esp_err_to_name(err));
                        }

                        Serial.println("🔄 Restarting to ESP_32_OTA bootloader...");
                        delay(2000);
                        ESP.restart();
                    }
                    else
                    {
                        Serial.printf("❌ Failed to set boot partition: %s\n", esp_err_to_name(err));
                    }
                }
                else
                {
                    Serial.println("❌ ota_0 partition not found!");
                }
            }
            else
            {
                Serial.println("Running on ota_0 - sensor unplugged handled by OTA bootloader");
            }

            // Fallback restart if partition switching fails
            delay(1000);
            ESP.restart();
        }

        sensorWasPresent = sensorCurrentlyPresent;
    }
}

// Handle backend cleanup
void handleBackendCleanup()
{
    if (backendCleanupRequested)
    {
        Serial.println("Backend cleanup requested");
        backendCleanupRequested = false;

        if (mqttConnected)
        {
            publishStatus("disconnected", "Rebooting to bootloader");
        }
        delay(1000);
        ESP.restart();
    }
}