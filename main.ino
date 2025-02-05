// Define pin numbers
const int ledPin = 2;      // LED connected to digital pin 8
const int ldrPin = A0;     // LDR connected to analog pin A0


void setup() 
{
  pinMode(ldrPin, INPUT);
  pinMode(ledPin, OUTPUT); // Set the LED pin as output
  Serial.begin(9600);      // Start serial communication at 9600 bps
}
 
void loop() 
{
  // Read the value from the LDR
  int value = analogRead(ldrPin);
  Serial.println(value);           // Print the value to the serial monitor
 
  // Assuming a lower value means more light
  if (value < 550)
  {   
    digitalWrite(ledPin, HIGH);  // Turn the LED on
  } 
  else 
  {
    digitalWrite(ledPin, LOW);   // Turn the LED off
  }
 
  delay(50);  // Wait for half a second
}