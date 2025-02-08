#include "FS.h"
#include "SPIFFS.h"
#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "ESP32_AccessPoint";
const char* password = "12345678";

WebServer server(80);

void handleRoot() {
    if (!SPIFFS.exists("/index.html")) {
        server.send(404, "text/plain", "Error: File /index.html not found on SPIFFS");
        Serial.println("Error: /index.html not found in SPIFFS.");
        return;
    }

    File file = SPIFFS.open("/index.html", "r");
    if (!file) {
        server.send(500, "text/plain", "Error: Unable to open /index.html");
        Serial.println("Error: Unable to open /index.html.");
        return;
    }

    server.streamFile(file, "text/html");
    file.close();
}

void setup() {
    Serial.begin(115200);

    // Initialize SPIFFS
    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS Mount Failed");
        return;
    }

    // Start WiFi in AP mode
    WiFi.softAP(ssid, password);
    Serial.println("Access Point Started");
    Serial.print("IP Address: ");
    Serial.println(WiFi.softAPIP());

    // Define server route
    server.on("/", handleRoot);
    server.begin();
}

void loop() {
    server.handleClient();
}
