#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <map>
// Defaults
#define RELAY_PIN 2
#define LDR_PIN 34

// int ON_THRESHOLD = 300;
// int OFF_THRESHOLD = 500;
// int SUNLIGHT_THRESHOLD = 600;
// int HYSTERESIS = 50;
// int SAMPLE_SIZE = 10;
// int READ_DELAY = 10;
// unsigned long NIGHT_MODE_DURATION = 300000;
// unsigned long DAY_MODE_DURATION = 1800000;

const char* ssid = "ALS0001";
const char* password = "123456789";

AsyncWebServer server(80);
Preferences prefs;

String processor(const String& var){
    std::map<String, String> preferences = {
        {"ON_THRESHOLD", "on_threshold"},
        {"OFF_THRESHOLD", "off_threshold"},
        {"SUN_THRESHOLD", "sun_threshold"},
        {"HYSTERESIS", "hysteresis"},
        {"SAMPLE_SIZE", "sample_size"},
        {"READ_DELAY", "read_delay"},
        {"NIGHT_MODE", "night_mode"},
        {"NIGHT_MODE_OFF", "night_mode_off"}
    };
    // if (var == "on_threshold"){
    //     return String(prefs.getInt("on_threshold", 300));
    // }
    if (preferences.count(var)) {
        return String(prefs.getInt(preferences[var].c_str(), 0));
    }
    return String();    
}

void setup() {
    Serial.begin(115200);
    prefs.begin("light_sys", false);
    loadPreferences();

    pinMode(LDR_PIN, INPUT);
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, HIGH);

    WiFi.softAP(ssid, password);
    Serial.print("AP IP address: ");
    Serial.println(WiFi.softAPIP());
    
    if (!LittleFS.begin()) {
        Serial.println("LittleFS Mount Failed!");
        return;
    }
    // int startValue = prefs.getInt("on_threshold", 300);
    // Serial.printf("Start value: %d\n", startValue);
    
    server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
        request->send(LittleFS, "/index.html", String(), false, processor);
    });

    server.on("/get", HTTP_GET, [](AsyncWebServerRequest *request) {
        Serial.println("Received Form Submission");
    
        if (request->hasParam("on_threshold") && request->getParam("on_threshold")->value() != "") {
            int value = request->getParam("on_threshold")->value().toInt();
            Serial.printf("On Threshold: %d\n", value);
            prefs.putInt("on_threshold", value);
        }
        if (request->hasParam("off_threshold") && request->getParam("off_threshold")->value() != "") {
            int value = request->getParam("off_threshold")->value().toInt();
            Serial.printf("Off Threshold: %d\n", value);
            prefs.putInt("off_threshold", value);
        }
        if (request->hasParam("sun_threshold") && request->getParam("sun_threshold")->value() != "") {
            int value = request->getParam("sun_threshold")->value().toInt();
            Serial.printf("Sunlight Threshold: %d\n", value);
            prefs.putInt("sun_threshold", value);
        }
        if (request->hasParam("hysteresis") && request->getParam("hysteresis")->value() != "") {
            int value = request->getParam("hysteresis")->value().toInt();
            Serial.printf("hysteresis: %d\n", value);
            prefs.putInt("hysteresis", value);
        }
        if (request->hasParam("sample_size") && request->getParam("sample_size")->value() != "") {
            int value = request->getParam("sample_size")->value().toInt();
            Serial.printf("Sample Size: %d\n", value);
            prefs.putInt("sample_size", value);
        }
        if (request->hasParam("read_delay") && request->getParam("read_delay")->value() != "") {
            int value = request->getParam("read_delay")->value().toInt();
            Serial.printf("Read Delay: %d\n", value);
            prefs.putInt("read_delay", value);
        }
        if (request->hasParam("night_mode") && request->getParam("night_mode")->value() != "") {
            int value = request->getParam("night_mode")->value().toInt();
            Serial.printf("Night Mode: %d\n", value);
            prefs.putInt("night_mode", value);
        }
        if (request->hasParam("night_mode_off") && request->getParam("night_mode_off")->value() != "") {
            int value = request->getParam("night_mode_off")->value().toInt();
            Serial.printf("Night Mode Off: %d\n", value);
            prefs.putInt("night_mode_off", value);
        }
    
        request->send(200, "text/plain", "Settings Saved!");
    });
    

    server.on("/reset", HTTP_GET, [](AsyncWebServerRequest *request) {
        resetToDefaults();
        loadPreferences();

        request->send(200, "text/plain", "Settings reset to defaults.");
    });

    server.begin();
    Serial.println("Server Started");
}

void loop() {}

void loadPreferences() {
    int onThreshold = prefs.getInt("on_threshold", 300);
    int offThreshold = prefs.getInt("off_threshold", 500);
    int sunThreshold = prefs.getInt("sun_threshold", 400);
    int hysteresis = prefs.getInt("hysteresis", 50);
    int sampleSize = prefs.getInt("sample_size", 10);
    int readDelay = prefs.getInt("read_delay", 10);
    int nightMode = prefs.getInt("night_mode", 300000);
    int nightModeOff = prefs.getInt("night_mode_off", 1800000);
    Serial.println("Settings loaded.");
}

void resetToDefaults() {
    prefs.putInt("on_threshold", 300);
    prefs.putInt("off_threshold", 500);
    prefs.putInt("sun_threshold", 400);
    prefs.putInt("hysteresis", 50);
    prefs.putInt("sample_size", 10);
    prefs.putInt("read_delay", 10);
    prefs.putInt("night_mode", 300000);
    prefs.putInt("night_mode_off", 1800000);
    Serial.println("Settings reset to defaults.");
}
