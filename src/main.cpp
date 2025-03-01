#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <AsyncWebSerial.h>
#include <map>

// Defaults
#define HEADLIGHT_PIN 12
#define LDR_PIN 34
#define ACC_PIN 33
#define PARKING_PIN 32
#define UNLOCK_PIN 35

std::map<String, int> settings = 
    {
        {"on_threshold", 300},
        {"off_threshold", 500},
        {"sun_threshold", 600},
        {"hysteresis", 50},
        {"sample_count", 10},
        {"read_delay", 10},
        {"night_mode", 300000},
        {"night_mode_off", 1800000},
        {"goodbye_lights", 15000},
        {"welcome_lights", 15000}
    };

std::map<String, int> defaults = settings;

const char* ssid = "ALS01";
const char* password = "123456789";

bool lightsOn = false;
unsigned long darkStartTime = 0;
unsigned long brightStartTime = 0;
bool isNightMode = false;

AsyncWebServer server(80);
AsyncWebSerial webSerial;

Preferences prefs;

void loadPreferences();
void savePreferences(AsyncWebServerRequest *request);
void resetToDefaults();
int readLDR();
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

bool checkLightCondition(int lightLevel){
  unsigned long currentTime = millis();
  
  if (lightLevel < settings["on_threshold"]) { //enters night mode if light level is below the on threshold for n seconds
    brightStartTime = 0;
    if (darkStartTime == 0) darkStartTime = currentTime;
    
    if (!isNightMode && (currentTime - darkStartTime >= settings["night_mode"])) {
      isNightMode = true;
      Serial.println("Night Mode Activated");
      webSerial.println("Night Mode Activated");
    }
    return true;
  } else {
    darkStartTime = 0;
    
    if (lightLevel >= settings["sun_threshold"]) { //exit night mode if sunlight is detected
      if (isNightMode) {
        isNightMode = false;
        Serial.println("Sunlight Detected - Exiting Night Mode");
        webSerial.println("Sunlight Detected - Exiting Night Mode");
      }
      return false;
    }
    
    if (lightLevel > settings["off_threshold"]) { //exits night mode is the light level is above the off threshold for n seconds
      if (brightStartTime == 0) brightStartTime = currentTime;
      
      if (isNightMode && (currentTime - brightStartTime >= settings["night_mode_off"])) {
        isNightMode = false;
        Serial.println("Exiting Night Mode...");
        webSerial.println("Exiting Night Mode...");
      }
    } else {
      brightStartTime = 0;
    }
    return !isNightMode ? false : lightsOn;
  }
}

void updateLights(bool shouldBeOn) {
  if (shouldBeOn != lightsOn) {
    digitalWrite(HEADLIGHT_PIN, shouldBeOn ? HIGH : LOW);
    lightsOn = shouldBeOn;
    Serial.println(lightsOn ? "Lights On" : "Lights Off");
    webSerial.println(lightsOn ? "Lights On" : "Lights Off");
  }
}

void debug(int lightLevel) {
  Serial.print("Light Level: ");
  Serial.println(lightLevel);
  Serial.print("Night Mode: ");
  Serial.println(isNightMode ? "Yes" : "No");
  Serial.print("Lights are: ");
  Serial.println(lightsOn ? "Yes" : "No");
  Serial.println();
  
  webSerial.print("Light Level: ");
  webSerial.println(lightLevel);
  webSerial.print("Night Mode: ");
  webSerial.println(isNightMode ? "Yes" : "No");
  webSerial.print("Lights are: ");
  webSerial.println(lightsOn ? "Yes" : "No");
  webSerial.println();
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
  pinMode(ACC_PIN, INPUT);
  pinMode(HEADLIGHT_PIN, OUTPUT);
  pinMode(PARKING_PIN, OUTPUT);

  digitalWrite(HEADLIGHT_PIN, HIGH);

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
  
  server.on("/reset", HTTP_POST, [](AsyncWebServerRequest *request) {
    resetToDefaults();
    request->send(200, "text/plain", "Settings reset to defaults.");
  });
  
  webSerial.begin(&server);
  server.begin();
  Serial.println("Server Started");
}

void loop() {
  int lightLevel = readLDR();
  bool shouldBeOn = checkLightCondition(lightLevel);
  updateLights(shouldBeOn);

  webSerial.loop();
  debug(lightLevel);
  delay(500);
}