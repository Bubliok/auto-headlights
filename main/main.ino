// Automatic Headlight Controller with Sunlight Detection
// Features: Immediate night mode exit on sunlight, time-based day/night detection

// Pins Configuration
#define LDR_PIN A0          // Light Dependent Resistor analog pin
#define RELAY_PIN 2         // Relay control pin (active-low relay)

// Threshold Configuration
#define ON_THRESHOLD 300    // Light level to turn lights ON (lower = darker)
#define OFF_THRESHOLD 500   // Light level to turn lights OFF (higher = lighter)
#define SUNLIGHT_THRESHOLD 600  // Immediate daylight detection level
#define HYSTERESIS 50       // Minimum difference between ON/OFF thresholds

// Filter Configuration
#define SAMPLE_SIZE 10      // Number of samples for moving average
#define READ_DELAY 10       // Delay between samples in milliseconds

// Time-Based Configuration
#define NIGHT_MODE_DURATION 300000  // 5 minutes to activate night mode
#define DAY_MODE_DURATION 1800000   // 30 minutes of brightness to exit night mode

bool lightsOn = false;
unsigned long darkStartTime = 0;
unsigned long brightStartTime = 0;
bool isNightMode = false;

void setup() {
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH); // Start with lights off
  
  Serial.begin(9600);
  Serial.println("Automatic Headlight System Initialized");
}

void loop() {
  int lightLevel = readFilteredLight();
  bool shouldBeOn = checkLightCondition(lightLevel);
  
  updateLights(shouldBeOn);
  debugOutput(lightLevel);
  delay(100);
}

int readFilteredLight() {
  static int samples[SAMPLE_SIZE];
  static int index = 0;
  static long sum = 0;
  
  sum -= samples[index];
  samples[index] = analogRead(LDR_PIN);
  sum += samples[index];
  index = (index + 1) % SAMPLE_SIZE;
  
  delay(READ_DELAY);
  return sum / SAMPLE_SIZE;
}

bool checkLightCondition(int lightLevel) {
  unsigned long currentTime = millis();

  // Darkness handling
  if (lightLevel < ON_THRESHOLD) {
    brightStartTime = 0;
    if (darkStartTime == 0) darkStartTime = currentTime;
    
    // Enter night mode after continuous darkness
    if (!isNightMode && (currentTime - darkStartTime >= NIGHT_MODE_DURATION)) {
      isNightMode = true;
      Serial.println("Night Mode Activated");
    }
    return true; // Always turn on lights when dark
  }
  // Brightness handling
  else {
    darkStartTime = 0;
    
    // Immediate sunlight detection
    if (lightLevel >= SUNLIGHT_THRESHOLD) {
      if (isNightMode) {
        isNightMode = false;
        Serial.println("Sunlight Detected - Exiting Night Mode");
      }
      return false;
    }
    
    // Time-based brightness detection
    if (lightLevel > OFF_THRESHOLD) {
      if (brightStartTime == 0) brightStartTime = currentTime;
      
      if (isNightMode && (currentTime - brightStartTime >= DAY_MODE_DURATION)) {
        isNightMode = false;
        Serial.println("Prolonged Brightness - Exiting Night Mode");
      }
    } else {
      brightStartTime = 0;
    }
    
    return !isNightMode ? false : lightsOn;
  }
}

void updateLights(bool shouldBeOn) {
  if (shouldBeOn != lightsOn) {
    digitalWrite(RELAY_PIN, shouldBeOn ? LOW : HIGH);
    lightsOn = shouldBeOn;
    Serial.println(lightsOn ? "Lights ON" : "Lights OFF");
  }
}

void debugOutput(int lightLevel) {
  Serial.print("Light: ");
  Serial.print(lightLevel);
  Serial.print(" | Status: ");
  Serial.print(lightsOn ? "ON" : "OFF");
  Serial.print(" | Night Mode: ");
  Serial.println(isNightMode ? "Yes" : "No");
}