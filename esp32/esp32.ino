#include <WiFi.h>
#include <Preferences.h>
#include <SPIFFS.h>

// Defaults
#define RELAY_PIN 2

int ON_THRESHOLD = 300;
int OFF_THRESHOLD = 500;
int SUNLIGHT_THRESHOLD = 600;
int HYSTERESIS = 50;
int SAMPLE_SIZE = 10;
int READ_DELAY = 10;
unsigned long NIGHT_MODE_DURATION = 300000;
unsigned long DAY_MODE_DURATION = 1800000;

const char* ssid = "ALS0001";
const char* password = "123456789";

WiFiServer server(80);
Preferences preferences;

void setup() {
    Serial.begin(115200);
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, HIGH);

    // Start WiFi AP Mode
    WiFi.softAP(ssid, password);
    Serial.println("Access Point Started");
    Serial.print("AP IP address: ");
    Serial.println(WiFi.softAPIP());
    
    // Start SPIFFS
    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS Mount Failed");
        return;
    }

    // Load Preferences
    preferences.begin("settings", false);
    ON_THRESHOLD = preferences.getInt("onThreshold", 300);
    OFF_THRESHOLD = preferences.getInt("offThreshold", 500);
    SUNLIGHT_THRESHOLD = preferences.getInt("sunThreshold", 600);
    HYSTERESIS = preferences.getInt("hysteresis", 50);
    SAMPLE_SIZE = preferences.getInt("sampleCount", 10);
    READ_DELAY = preferences.getInt("sampleDelay", 10);
    NIGHT_MODE_DURATION = preferences.getULong("nightMode", 300000);
    DAY_MODE_DURATION = preferences.getULong("nightModeExit", 1800000);
    preferences.end();

    server.begin();
    Serial.println("Server Started");
}

void loop() {
    WiFiClient client = server.available();  // Check for incoming clients

    if (client) {
        Serial.println("New Client Connected");
        String request = "";
        while (client.connected()) {
            if (client.available()) {
                char c = client.read();
                request += c;
                if (c == '\n') {
                    if (request.length() == 2) {  // Empty line received (End of HTTP request)
                        if (request.indexOf("GET /submit") >= 0) {
                            handleFormSubmission(request);
                            sendRedirect(client);
                        } else if (request.indexOf("GET /getSettings") >= 0) {
                            serveSettings(client);  // Serve settings JSON
                        } else {
                            servePage(client);
                        }
                        break;
                    }
                    request = "";  // Reset for next line
                }
            }
        }
        client.stop();
        Serial.println("Client Disconnected");
    }
}


void servePage(WiFiClient& client) {
    if (!SPIFFS.exists("/index.html")) {
        Serial.println("Error: /index.html not found in SPIFFS.");
        client.println("HTTP/1.1 404 Not Found");
        client.println("Content-type:text/plain");
        client.println("Connection: close");
        client.println();
        client.println("404 - File Not Found");
        return;
    }

    File file = SPIFFS.open("/index.html", "r");
    if (!file) {
        Serial.println("Error: Unable to open /index.html.");
        client.println("HTTP/1.1 500 Internal Server Error");
        client.println("Content-type:text/plain");
        client.println("Connection: close");
        client.println();
        client.println("500 - Internal Server Error");
        return;
    }

    client.println("HTTP/1.1 200 OK");
    client.println("Content-type:text/html");
    client.println("Connection: close");
    client.println();

    while (file.available()) {
        client.write(file.read());
    }
    file.close();
}

void handleFormSubmission(String request) {
    preferences.begin("settings", false);
    preferences.putInt("onThreshold", getValueFromQuery(request, "onThreshold"));
    preferences.putInt("offThreshold", getValueFromQuery(request, "offThreshold"));
    preferences.putInt("sunThreshold", getValueFromQuery(request, "sunThreshold"));
    preferences.putInt("hysteresis", getValueFromQuery(request, "hysteresis"));
    preferences.putInt("sampleCount", getValueFromQuery(request, "sampleCount"));
    preferences.putInt("sampleDelay", getValueFromQuery(request, "sampleDelay"));
    preferences.putULong("nightMode", getValueFromQuery(request, "nightMode"));
    preferences.putULong("nightModeExit", getValueFromQuery(request, "nightModeExit"));
    preferences.end();
}

void sendRedirect(WiFiClient& client) {
    client.println("HTTP/1.1 303 See Other");
    client.println("Location: /");
    client.println("Connection: close");
    client.println();
}

int getValueFromQuery(String query, String key) {
    int startIndex = query.indexOf(key + "=");
    if (startIndex < 0) return 0;
    startIndex += key.length() + 1;
    int endIndex = query.indexOf("&", startIndex);
    if (endIndex < 0) endIndex = query.length();
    return query.substring(startIndex, endIndex).toInt();
}
void serveSettings(WiFiClient& client) {
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: application/json");
    client.println("Connection: close");
    client.println();
    
    client.print("{");
    client.print("\"onThreshold\":"); client.print(ON_THRESHOLD); client.print(",");
    client.print("\"offThreshold\":"); client.print(OFF_THRESHOLD); client.print(",");
    client.print("\"sunThreshold\":"); client.print(SUNLIGHT_THRESHOLD); client.print(",");
    client.print("\"hysteresis\":"); client.print(HYSTERESIS); client.print(",");
    client.print("\"sampleCount\":"); client.print(SAMPLE_SIZE); client.print(",");
    client.print("\"sampleDelay\":"); client.print(READ_DELAY); client.print(",");
    client.print("\"nightMode\":"); client.print(NIGHT_MODE_DURATION); client.print(",");
    client.print("\"nightModeExit\":"); client.print(DAY_MODE_DURATION);
    client.println("}");
}
