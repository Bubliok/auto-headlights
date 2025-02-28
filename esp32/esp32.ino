#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <map>

// Defaults
#define RELAY_PIN 2
#define LDR_PIN 34

std::map<String, int> settings = 
    {   
        {"on_threshold", 300},
        {"off_threshold", 500},
        {"sun_threshold", 600},
        {"hysteresis", 50},
        {"sample_size", 10},
        {"read_delay", 10},
        {"night_mode", 300000},
        {"night_mode_off", 1800000},
        {"goodbye_lights", 15000},
        {"welcome_lights", 15000}
    };

const char* ssid = "ALS0001";
const char* password = "123456789";

AsyncWebServer server(80);
Preferences prefs;

String processor(const String& var){
    if (settings.count(var)) {
        return String(settings[var]);
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
    
    server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
        request->send(LittleFS, "/index.html", String(), false, processor);
    });

    server.on("/get", HTTP_GET, savePreferences);
    
    server.on("/reset", HTTP_GET, [](AsyncWebServerRequest *request) {
        resetToDefaults();
        loadPreferences();

        request->send(200, "text/plain", "Settings reset to defaults.");
    });

    server.begin();
    Serial.println("Server Started");
}

void loop() {}

void savePreferences(AsyncWebServerRequest *request) {
    for (auto &pair : settings) {
        if (request -> hasParam(pair.first) && request->getParam(pair.first)->value() != "") {
            int value = request->getParam(pair.first)->value().toInt();
            prefs.putInt(pair.first.c_str(), value);
            settings[pair.first] = value;
        }
    }
    request->send(200, "text/plain", "Settings saved.");
}

void loadPreferences() {
    for (auto &pair : settings) {
        settings[pair.first] = prefs.getInt(pair.first.c_str(), pair.second);
    }
    Serial.println("Settings loaded.");
}

void resetToDefaults() {
    for (auto &pair : settings) {
        prefs.putInt(pair.first.c_str(), pair.second);
    }
    Serial.println("Defaults loaded.");
}
