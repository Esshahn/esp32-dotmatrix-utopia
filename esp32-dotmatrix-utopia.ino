#include "ESP32-HUB75-MatrixPanel-I2S-DMA.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <Arduino_JSON.h>
#include "time.h"
#include "env.h"

// Configuration constants
constexpr uint8_t DISPLAY_WIDTH = 64;
constexpr uint8_t DISPLAY_HEIGHT = 64;
constexpr uint8_t BRIGHTNESS = 100;  // 0-255
constexpr uint8_t MAX_WIFI_ATTEMPTS = 20;
constexpr unsigned long REFRESH_INTERVAL = 1000UL * 60 * 60;  // 1 hour in milliseconds
constexpr unsigned long SLEEP_DURATION = 1000000ULL * 60 * 10;  // 10 minutes in microseconds

// Time configuration
constexpr char NTP_SERVER[] = "pool.ntp.org";
constexpr long GMT_OFFSET_SEC = 3600;
constexpr int DAYLIGHT_OFFSET_SEC = 3600;
constexpr uint8_t ACTIVE_HOURS_START = 7;
constexpr uint8_t ACTIVE_HOURS_END = 22;

class DisplayManager {
private:
    MatrixPanel_I2S_DMA* display;
    bool isActive = true;
    unsigned long lastUpdateTime = 0;

public:
    DisplayManager() {
        HUB75_I2S_CFG config(DISPLAY_WIDTH, DISPLAY_HEIGHT, 1);
        config.gpio.e = 32;
        config.clkphase = false;
        config.driver = HUB75_I2S_CFG::FM6124;

        display = new MatrixPanel_I2S_DMA(config);
        display->begin();
        display->setBrightness8(BRIGHTNESS);
        display->clearScreen();
        display->setTextSize(1);
    }

    void setTextColor(uint8_t r, uint8_t g, uint8_t b) {
        display->setTextColor(display->color444(r, g, b));
    }

    void showStartupMessage() {
        setTextColor(2, 6, 2);
        display->setCursor(3, 3);
        display->println("utopia");
    }

    void drawText(const String& text, uint8_t x, uint8_t y) {
        const uint8_t LONG_WORD_LENGTH = 10;
        const uint8_t SHORT_LINE_HEIGHT = 9;
        const uint8_t LONG_LINE_HEIGHT = 18;

        char* words = strdup(text.c_str());
        char* word = strtok(words, " ");
        uint8_t currentY = y;

        while (word != nullptr) {
            display->setCursor(x, currentY);
            display->println(word);
            currentY += (strlen(word) > LONG_WORD_LENGTH) ? LONG_LINE_HEIGHT : SHORT_LINE_HEIGHT;
            word = strtok(nullptr, " ");
        }

        free(words);
    }

    void updateDisplay(const String& text) {
        Serial.println("Updating display with: " + text);
        display->clearScreen();
        setTextColor(random(0, 15), random(0, 15), random(0, 15));
        drawText(text, 2, 2);
    }

    bool shouldUpdate() {
        unsigned long currentTime = millis();
        bool timeToUpdate = (currentTime - lastUpdateTime > REFRESH_INTERVAL);
        Serial.printf("Time since last update: %lu ms, Should update: %d\n", 
                     currentTime - lastUpdateTime, timeToUpdate);
        return isActive && timeToUpdate;
    }

    void markUpdated() {
        lastUpdateTime = millis();
        Serial.println("Display update marked at: " + String(lastUpdateTime));
    }

    void sleep() {
        Serial.println("Entering deep sleep mode...");
        isActive = false;
        display->clearScreen();  // Clear display before sleep
        display->setBrightness8(0);  // Turn off display
        esp_sleep_enable_timer_wakeup(SLEEP_DURATION);
        esp_deep_sleep_start();
    }

    void activate() {
        isActive = true;
        display->setBrightness8(BRIGHTNESS);
        Serial.println("Display activated");
    }

    void showDebugMessage(const String& msg) {
        display->clearScreen();
        setTextColor(15, 0, 0);  // Bright red for debug
        drawText(msg, 2, 2);
        Serial.println("Debug message: " + msg);
    }
};

class NetworkManager {
private:
    WiFiClient client;
    HTTPClient http;

public:
    bool connectToWiFi() {
        Serial.println("Connecting to WiFi...");
        WiFi.begin(ssid, password);
        uint8_t attempts = 0;

        while (WiFi.status() != WL_CONNECTED && attempts < MAX_WIFI_ATTEMPTS) {
            delay(1000);
            Serial.print(".");
            attempts++;
        }

        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\nConnected to WiFi");
            Serial.println(WiFi.localIP());
            return true;
        }
        
        Serial.println("\nWiFi connection failed");
        return false;
    }

    String fetchMessage() {
        if (WiFi.status() != WL_CONNECTED) {
            if (!connectToWiFi()) {
                return "WiFi Error";
            }
        }

        Serial.println("Fetching message from: " + String(serverName));
        http.begin(client, serverName);
        int httpCode = http.GET();
        
        String message;
        if (httpCode > 0) {
            String payload = http.getString();
            Serial.println("Response: " + payload);
            
            JSONVar response = JSON.parse(payload);
            if (JSON.typeof(response) != "undefined") {
                String jsonString = JSON.stringify(response["msg"]);
                message = jsonString.substring(1, jsonString.length() - 1);
            } else {
                message = "JSON Error";
            }
        } else {
            message = "HTTP Error " + String(httpCode);
        }
        
        http.end();
        return message;
    }
};

class TimeManager {
public:
    static bool isActiveHours() {
        struct tm timeinfo;
        if (!getLocalTime(&timeinfo)) {
            Serial.println("Failed to get time - check NTP connection");
            display.showDebugMessage("Time Error");
            return false;  // Changed to false to prevent always-on behavior
        }

        bool isActive = timeinfo.tm_hour >= ACTIVE_HOURS_START && 
                       timeinfo.tm_hour < ACTIVE_HOURS_END;  // Changed <= to < for exact cutoff
        
        Serial.printf("Current hour: %d, Active hours: %d-%d, Is active: %d\n", 
                     timeinfo.tm_hour, ACTIVE_HOURS_START, ACTIVE_HOURS_END, isActive);
        
        return isActive;
    }

    static bool initialize() {
        configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
        
        // Wait for initial time sync with timeout
        int timeoutCounter = 0;
        struct tm timeinfo;
        while (!getLocalTime(&timeinfo) && timeoutCounter < 10) {
            Serial.println("Waiting for time sync...");
            delay(1000);
            timeoutCounter++;
        }
        
        if (timeoutCounter >= 10) {
            Serial.println("Time sync failed");
            return false;
        }
        
        Serial.printf("Time initialized. Current time: %02d:%02d:%02d\n", 
                     timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        return true;
    }
};

// Global objects
DisplayManager display;
NetworkManager network;

void setup() {
    Serial.begin(115200);
    Serial.println("\nStarting up...");
    
    randomSeed(analogRead(0));
    display.showStartupMessage();
    
    // Initial WiFi connection
    if (!network.connectToWiFi()) {
        display.showDebugMessage("WiFi Failed");
        delay(3000);
        ESP.restart();
    }
    
    // Initialize time synchronization
    if (!TimeManager::initialize()) {
        display.showDebugMessage("Time Sync Failed");
        delay(3000);
        ESP.restart();
    }
    
    // Check if we should be active at startup
    if (!TimeManager::isActiveHours()) {
        Serial.println("Outside active hours at startup, going to sleep");
        display.showDebugMessage("Sleep Time");
        delay(3000);
        display.sleep();
    }
    
    // Initial display update if we're active
    String initialMessage = network.fetchMessage();
    if (!initialMessage.isEmpty()) {
        display.updateDisplay(initialMessage);
    }
    display.markUpdated();
}

void loop() {
    if (display.shouldUpdate()) {
        Serial.println("Time for update");
        
        if (TimeManager::isActiveHours()) {
            display.activate();
            String message = network.fetchMessage();
            if (!message.isEmpty()) {
                display.updateDisplay(message);
            }
            display.markUpdated();
        } else {
            Serial.println("Outside active hours, going to sleep");
            display.showDebugMessage("Sleep Time");
            delay(3000);
            display.sleep();
        }
    }
    delay(1000);  // Small delay to prevent tight looping
}