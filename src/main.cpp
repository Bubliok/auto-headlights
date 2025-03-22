#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <AsyncWebSerial.h>
#include <ElegantOTA.h>
#include <map>

// Defaults
#define HEADLIGHT_PIN 25
#define LDR_PIN 34
#define IGN_PIN 33
#define PARKING_PIN 32
#define UNLOCK_PIN 35

std::map<String, int> settings = 
    {
        {"on_threshold", 1700},
        {"off_threshold", 2000},
        {"hysteresis", 300},
        {"sample_count", 25},
        {"read_delay", 200},
        {"goodbye_lights", 30000},
        {"welcome_lights", 15000}
    };

std::map<String, int> defaults = settings;

const char* ssid = "ALS01";
const char* password = "123456789";

bool lightsOn = false;
unsigned long ignOffTime = 0;
bool ignTimeoutActive = false;
bool ignOverride = false;
bool ignWasOn = false;
bool manualOverride = false;


AsyncWebServer server(80);
AsyncWebSerial webSerial;

Preferences prefs;

void loadPreferences();
void savePreferences(AsyncWebServerRequest *request);
void resetToDefaults();
int readLDR();
void goodbyeLights(int lightLevel);
bool checkLightCondition(int lightLevel);
void updateLights(bool shouldBeOn);
void debug(int lightLevel);

String processor(const String& var){
    if (settings.count(var)) {
        return String(settings[var]);
    }
    return String();
}
  
  int readLDR() { // reads the light sensor value and returns the average of the last n samples
    static std::vector<int> samples(settings["sample_count"], 0);
    static int i = 0;
    static long sum = 0;
    
    if (samples.size() != settings["sample_count"]) {
      samples.assign(settings["sample_count"], 0);
      sum = 0;
      i = 0;
    }
    
    sum -= samples[i];
    samples[i] = analogRead(LDR_PIN);
    sum += samples[i];
    i = (i + 1) % settings["sample_count"];
    
    delay(settings["read_delay"]);
    
    return sum / settings["sample_count"];
  }

  void goodbyeLights(int lightLevel) { // monitors the IGN state and turns off the lights after a delay
    bool ignState = digitalRead(IGN_PIN);
  
    if (ignState) {
        ignWasOn = true;
        ignTimeoutActive = false;
        ignOverride = false; 
    } else {
        if (ignWasOn) { 
            ignWasOn = false;
            ignOffTime = millis();
            ignTimeoutActive = true;
            Serial.println("ACC turned off, activating goodbye lights...");
            webSerial.println("ACC turned off, activating goodbye lights...");
        }
  
        if (ignTimeoutActive) {
            if (millis() - ignOffTime < settings["goodbye_lights"]) {
              if (lightLevel < settings["on_threshold"]) {
                updateLights(true);
              } else {
               digitalWrite(PARKING_PIN, HIGH);
              }
            } else {
                ignTimeoutActive = false;
                ignOverride = true;
                updateLights(false);
                digitalWrite(PARKING_PIN, LOW);
                Serial.println("Goodbye delay over, turning off lights.");
                webSerial.println("Goodbye delay over, turning off lights.");
            }
        }
    }
  }

  void welcomeLights(int lightLevel) {
    static bool welcomeLightsActive = false;
    static bool headlightsActive = false;
    static unsigned long welcomeStartTime = 0;
    static unsigned long headlightsStartTime = 0;

    if (digitalRead(UNLOCK_PIN) == HIGH) {
        webSerial.println("Unlock signal detected.");

        digitalWrite(PARKING_PIN, HIGH); 
        welcomeLightsActive = true;
        welcomeStartTime = millis();

        if (lightLevel < settings["on_threshold"]) { 
            webSerial.println("Dark environment detected, turning on headlights.");
            
            digitalWrite(HEADLIGHT_PIN, HIGH);
            headlightsActive = true;
            headlightsStartTime = millis();
        }
    }

    if (headlightsActive && millis() - headlightsStartTime >= 10000) {
        digitalWrite(HEADLIGHT_PIN, LOW);
        headlightsActive = false;
        webSerial.println("Headlights timeout, turning off.");
    }

    if (welcomeLightsActive && millis() - welcomeStartTime >= settings["welcome_lights"]) {
        digitalWrite(PARKING_PIN, LOW);
        welcomeLightsActive = false;
        webSerial.println("Welcome lights timeout, turning off.");
    }
}
  
  bool checkLightCondition(int lightLevel) {
    static unsigned long brightStartTime = 0;
    unsigned long currentTime = millis();

    if (lightLevel < settings["on_threshold"]) {
        brightStartTime = 0;
        return true;
    } else if (lightLevel > settings["off_threshold"]) {
        if (brightStartTime == 0) {
            brightStartTime = currentTime;
        }
        if (currentTime - brightStartTime >= 3000) {
            return false;
        }
        return lightsOn;
    }
    return lightsOn;
}

void updateLights(bool shouldBeOn) {
  if (manualOverride) {
    webSerial.println("Manual override active.");
    return;
  }

  if (!digitalRead(IGN_PIN) && !ignTimeoutActive && shouldBeOn) {
    Serial.println("IGN off, lights off.");
    webSerial.println("IGN off, lights off.");
    return;
  }

  if (ignOverride && shouldBeOn) {
      Serial.println("IGN off.");
      webSerial.println("IGN off.");
      return; 
  }

  if (shouldBeOn) {
      if (!lightsOn) {
          digitalWrite(HEADLIGHT_PIN, HIGH);
          digitalWrite(PARKING_PIN, HIGH);
          lightsOn = true;
          Serial.println("Lights On");
          webSerial.println("Lights On");
      }
  } else {
      if (lightsOn) {
          digitalWrite(HEADLIGHT_PIN, LOW);
          digitalWrite(PARKING_PIN, LOW);
          lightsOn = false;
          Serial.println("Lights Off");
          webSerial.println("Lights Off");
      }
  }
}

void setManualLights(bool headlights, bool parking) {
  if (!manualOverride) return;

  digitalWrite(HEADLIGHT_PIN, headlights ? HIGH : LOW);
  digitalWrite(PARKING_PIN, parking ? HIGH : LOW);

  Serial.print("Manual Control - Headlights: ");
  Serial.print(headlights ? "ON" : "OFF");
  Serial.print(", Parking: ");
  Serial.println(parking ? "ON" : "OFF");

  webSerial.print("Manual Control - Headlights: ");
  webSerial.print(headlights ? "ON" : "OFF");
  webSerial.print(", Parking: ");
  webSerial.println(parking ? "ON" : "OFF");
}

void setManualOverride(bool enable) {
  manualOverride = enable;

  if (enable) {
    Serial.println("Manual override enabled.");
    webSerial.println("Manual override enabled.");
  } else {
    digitalWrite(HEADLIGHT_PIN, LOW);
    digitalWrite(PARKING_PIN, LOW);
    lightsOn = false;
    Serial.println("Exiting Manual Mode, Resetting Lights");
    webSerial.println("Exiting Manual Mode, Resetting Lights");
  }
}



void debug(int lightLevel) {
  Serial.print("Light Level: ");
  Serial.println(lightLevel);
  // Serial.print("Night Mode: ");
  // Serial.println(isNightMode ? "Yes" : "No");
  // Serial.print("Lights are: ");
  // Serial.println(lightsOn ? "Yes" : "No");
  // Serial.println();
  
  webSerial.print("Light Level: ");
  webSerial.println(lightLevel);
  // webSerial.print("Night Mode: ");
  // webSerial.println(isNightMode ? "Yes" : "No");
  // webSerial.print("Lights are: ");
  // webSerial.println(lightsOn ? "Yes" : "No");
  // webSerial.println();
  // bool accState = digitalRead(ACC_PIN);
  // Serial.print("ACC State: ");
  // Serial.println(accState);

}

// -------------------------------------------------------------

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
    webSerial.println("Settings loaded.");
}

void resetToDefaults() {
    for (auto &pair : defaults) {
        prefs.putInt(pair.first.c_str(), pair.second);
    }
    loadPreferences();
    Serial.println("Defaults loaded.");
    webSerial.println("Defaults loaded.");
}

void setup() {
  Serial.begin(115200);
  prefs.begin("light_sys", false);
  loadPreferences();

  pinMode(LDR_PIN, INPUT);
  pinMode(UNLOCK_PIN, INPUT);
  pinMode(IGN_PIN, INPUT_PULLDOWN);
  pinMode(HEADLIGHT_PIN, OUTPUT);
  pinMode(PARKING_PIN, OUTPUT);

  digitalWrite(HEADLIGHT_PIN, LOW);
  digitalWrite(PARKING_PIN, LOW);
  lightsOn = false;
  manualOverride = false;
  ignOverride = false;
  ignTimeoutActive = false;
  ignWasOn = false;

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

  server.on("/styles.css", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(LittleFS, "/styles.css", "text/css");
  });
  
  server.on("/get", HTTP_GET, savePreferences);
  
  server.on("/reset", HTTP_POST, [](AsyncWebServerRequest *request) {
    resetToDefaults();
    request->send(200, "text/plain", "Settings reset to defaults.");
  });
  
  server.on("/ignstatus", HTTP_GET, [](AsyncWebServerRequest *request) {
    bool ignState = digitalRead(IGN_PIN);
    String response = "{\"status\":" + String(ignState ? "true" : "false") + "}";
    request->send(200, "application/json", response);
  });
  
  server.on("/manual-mode", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("enabled")) {
      bool enabled = request->getParam("enabled")->value() == "true";
      setManualOverride(enabled); 
  
      String response = "{\"success\":true,\"manualOverride\":" + String(enabled ? "true" : "false") + "}";
      request->send(200, "application/json", response);
    } else {
      request->send(400, "text/plain", "Bad request");
    }
  });

  server.on("/headlights-toggle", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (manualOverride) {
      bool currentState = digitalRead(HEADLIGHT_PIN);
      setManualLights(!currentState, digitalRead(PARKING_PIN));
      String response = "{\"success\":true,\"lightsOn\":" + String(!currentState ? "true" : "false") + "}";
      request->send(200, "application/json", response);
    } else {
      request->send(400, "application/json", "{\"success\":false,\"message\":\"Manual override not enabled\"}");
    }
  });

  server.on("/parkinglights-toggle", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (manualOverride) {
      bool currentState = digitalRead(PARKING_PIN);
      setManualLights(digitalRead(HEADLIGHT_PIN), !currentState);
      String response = "{\"success\":true,\"parkingLightsOn\":" + String(!currentState ? "true" : "false") + "}";
      request->send(200, "application/json", response);
    } else {
      request->send(400, "application/json", "{\"success\":false,\"message\":\"Manual override not enabled\"}");
    }
  });
  webSerial.begin(&server);
  server.begin();
  Serial.println("Server Started");
  ElegantOTA.begin(&server);
}

void loop() {
  int lightLevel = readLDR();
  bool shouldBeOn = checkLightCondition(lightLevel);

  goodbyeLights(lightLevel);
  welcomeLights(lightLevel);
  updateLights(shouldBeOn);

  ElegantOTA.loop();
  webSerial.loop();
  debug(lightLevel);
  delay(200);
}