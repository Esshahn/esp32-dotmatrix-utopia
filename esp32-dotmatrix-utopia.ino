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
        return isActive && (millis() - lastUpdateTime > REFRESH_INTERVAL);
    }

    void markUpdated() {
        lastUpdateTime = millis();
    }

    void sleep() {
        isActive = false;
        esp_sleep_enable_timer_wakeup(SLEEP_DURATION);
        esp_deep_sleep_start();
    }

    void activate() {
        isActive = true;
        display->setBrightness8(BRIGHTNESS);
    }

    void showDebugMessage(const String& msg) {
        display->clearScreen();
        setTextColor(15, 0, 0);  // Bright red for debug
        drawText(msg, 2, 2);
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
            Serial.println("Failed to get time");
            return true;  // Default to active if time fetch fails
        }
        Serial.printf("Current hour: %d\n", timeinfo.tm_hour);
        return timeinfo.tm_hour >= ACTIVE_HOURS_START && 
               timeinfo.tm_hour <= ACTIVE_HOURS_END;
    }

    static void initialize() {
        configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
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
    
    TimeManager::initialize();
    delay(1000);  // Give time for initial time sync
    
    // Force first update
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
        } else {
            Serial.println("Outside active hours, going to sleep");
            display.sleep();
        }
        
        display.markUpdated();
    }
    delay(1000);  // Small delay to prevent tight looping
}