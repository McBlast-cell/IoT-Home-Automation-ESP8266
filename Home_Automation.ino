/*
 ============================================================================
   IoT HOME AUTOMATION 
   Hardware: NodeMCU ESP8266 + DHT22 + SH1106 OLED + Relay + RGB LED
   Platform: Blynk IoT
 ============================================================================
*/

// ── 1. BLYNK & WIFI CONFIGURATION ─────────────────────────────────────────
// These definitions link your physical device to your specific Blynk cloud dashboard.
#define BLYNK_TEMPLATE_ID "TMPL3o79qOzNg"
#define BLYNK_TEMPLATE_ID   "TMPL**********"     // Blynk Template ID (hidden for security)
#define BLYNK_AUTH_TOKEN    "****************************"  // Auth Token (hidden for security)

// Local Wi-Fi credentials
char ssid[] = "******************";   // WiFi SSID (hidden for security)
char pass[] = "******************";   // WiFi Password (hidden for security)

// ── 2. LIBRARIES ──────────────────────────────────────────────────────────
#include <ESP8266WiFi.h>      // Enables Wi-Fi capability for ESP8266
#include <BlynkSimpleEsp8266.h> // Handles Blynk cloud communication
#include <DHT.h>              // Controls the DHT22 Temperature & Humidity sensor
#include <U8g2lib.h>           // Advanced library to draw graphics/text on OLED
#include <Wire.h>             // Enables I2C communication (used for OLED screen)

// ── 3. HARDWARE PIN DEFINITIONS ───────────────────────────────────────────
// Assigning human-readable names to the NodeMCU GPIO pins
#define DHT_PIN     2    // GPIO2 maps to pin D4 on the NodeMCU
#define DHT_TYPE    DHT22// Specifying the precise sensor model (DHT22)
#define RELAY_PIN   14   // GPIO14 maps to pin D5 (Active LOW: 0V turns it ON)
#define LED_RED     12   // GPIO12 maps to pin D6
#define LED_GREEN   13   // GPIO13 maps to pin D7
#define LED_BLUE    15   // GPIO15 maps to pin D8

// ── 4. OBJECT INITIALIZATION ──────────────────────────────────────────────
DHT dht(DHT_PIN, DHT_TYPE); // Creates the DHT sensor object

// Initializes the SH1106 OLED screen using standard Hardware I2C (Pins D1/SCL and D2/SDA)
U8G2_SH1106_128X64_NONAME_F_HW_I2C oled(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

BlynkTimer timer; // Timer object to run tasks (like reading sensors) without blocking the loop

// ── 5. GLOBAL STATE VARIABLES ─────────────────────────────────────────────
bool  relayState    = false; // Tracks if relay is ON (true) or OFF (false)
bool  autoMode      = false; // Tracks if Auto Mode is enabled via Blynk
float tempThreshold = 30.0;  // Target temperature to trigger the relay in Auto Mode
float currentTemp   = 0.0;   // Stores the latest temperature reading
float currentHumid  = 0.0;   // Stores the latest humidity reading

// ── 6. HELPER FUNCTIONS ───────────────────────────────────────────────────

/**
 * Updates the physical state of the RGB LED.
 * Accepts true/false for Red, Green, and Blue channels.
 */
void setRGB(bool r, bool g, bool b) {
  digitalWrite(LED_RED,   r ? HIGH : LOW);
  digitalWrite(LED_GREEN, g ? HIGH : LOW);
  digitalWrite(LED_BLUE,  b ? HIGH : LOW);
}

/**
 * Main controller for turning the Relay ON or OFF.
 * Also changes RGB colors dynamically to reflect system states.
 * @param state - true for ON, false for OFF
 * @param src - Text string representing what triggered the change (for debugging)
 */
void setRelay(bool state, String src) {
  relayState = state;
  // Relays are active LOW; writing LOW completes the circuit (turns it ON)
  digitalWrite(RELAY_PIN, state ? LOW : HIGH);

  // Color Coding Status:
  if (autoMode && state)    setRGB(1,1,0);  // Yellow = Auto Mode running device
  else if (state)           setRGB(0,1,0);  // Green  = Manual Mode ON
  else                      setRGB(1,0,0);  // Red    = Relay is OFF

  // Sync the physical change back to the Blynk cloud app (Virtual Pin V0)
  Blynk.virtualWrite(V0, state ? 1 : 0);
  Serial.println("Relay " + String(state ? "ON" : "OFF") + " [" + src + "]");
}

/**
 * Draws the user interface layout on the 128x64 OLED display.
 */
void updateOLED() {
  oled.clearBuffer();             // Clear the internal display memory
  oled.setFont(u8g2_font_6x10_tf); // Set a clear, small text font

  // Row 1: Header title
  oled.drawStr(15, 10, "HOME AUTOMATION");
  oled.drawHLine(0, 12, 128);     // Draw a clean dividing line

  // Row 2: Live Temperature
  oled.drawStr(0, 23, "Temperature:");
  oled.drawStr(75, 23, (String(currentTemp, 1) + " C").c_str());

  // Row 3: Live Humidity
  oled.drawStr(0, 33, "Humidity:");
  oled.drawStr(75, 33, (String(currentHumid, 1) + " %").c_str());

  // Row 4: Comfort feel assessment
  oled.drawStr(0, 43, "Feel:");
  if (currentTemp > 25.0) {
    oled.drawStr(75, 43, "Warm");
  } else {
    oled.drawStr(75, 43, "Cold");
  }

  // Row 5: Relay State
  oled.drawStr(0, 53, "Relay:");
  oled.drawStr(75, 53, relayState ? "ON" : "OFF");

  // Row 6: Automode State
  oled.drawStr(0, 63, "Auto Mode:");
  oled.drawStr(75, 63, autoMode ? "ON" : "OFF");

  oled.sendBuffer();              // Push the drawn images to the physical screen
}

// ── 7. BLYNK VIRTUAL PIN LISTENERS ────────────────────────────────────────

/**
 * Listens to Virtual Pin V0 (Relay Switch Widget on Blynk App)
 * Triggers every time you tap the button in the app.
 */
BLYNK_WRITE(V0) {
  if (!autoMode) {
    // If auto mode is off, obey manual controls from the app
    setRelay(param.asInt() == 1, "Blynk app");
    updateOLED(); 
  } else {
    // If auto mode is active, reject the change and force the app button back to current state
    Blynk.virtualWrite(V0, relayState ? 1 : 0);
    Serial.println("Manual blocked — auto mode ON");
  }
}

/**
 * Listens to Virtual Pin V3 (Auto Mode Slider/Switch Widget on Blynk App)
 */
BLYNK_WRITE(V3) {
  autoMode = param.asInt() == 1; // Update global state variable
  Serial.println("Auto mode: " + String(autoMode ? "ON" : "OFF"));
  
  if (autoMode) {
    setRGB(0,0,1);  // Blue indicates "Auto Mode Standby / Waiting"
  } else {
    // Revert back to Green (ON) or Red (OFF) depending on current relay state
    setRGB(relayState ? 0 : 1, relayState ? 1 : 0, 0);
  }
  updateOLED(); 
}

/**
 * Listens to Virtual Pin V4 (Threshold Slider Widget on Blynk App)
 * Adjusts the target temperature dynamically from the app.
 */
BLYNK_WRITE(V4) {
  tempThreshold = param.asFloat();
  Serial.println("Threshold: " + String(tempThreshold) + "C");
}

// ── 8. CORE LOGIC (SENSOR READING & AUTOMATION) ───────────────────────────
/**
 * Main routine executed periodically by the timer.
 * Gathers environment data, updates cloud, handles safety cutoffs.
 */
void readSensor() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();

  // Fail-safe protection check: Stop execution if wire disconnected or hardware error
  if (isnan(t) || isnan(h)) {
    Serial.println("DHT22 read error — check wiring");
    return;
  }

  currentTemp  = t;
  currentHumid = h;

  // Stream current values to the Blynk App Gauges/Graphs
  Blynk.virtualWrite(V1, currentTemp);
  Blynk.virtualWrite(V2, currentHumid);

  // Auto Automation Engine Logic
  if (autoMode) {
    // If it gets too hot, turn on the appliance (e.g., Fan/AC connected to Relay)
    if (currentTemp >= tempThreshold && !relayState) {
      setRelay(true, "Auto HIGH temp");
    } 
    // Hysteresis calculation: Prevents rapid flickering by requiring a 2°C cooling buffer before shutoff
    else if (currentTemp < (tempThreshold - 2.0) && relayState) {
      setRelay(false, "Auto LOW temp");
    }
  }

  updateOLED(); // Refresh the screen with new sensor metrics

  // Local Debug printouts to Serial Monitor
  Serial.print("T:" + String(currentTemp, 1) + "C  ");
  Serial.print("H:" + String(currentHumid, 1) + "%  ");
  Serial.println("Relay:" + String(relayState ? "ON" : "OFF"));
}

// ── 9. SYSTEM SETUP (RUNS ONCE ON POWER UP) ───────────────────────────────
void setup() {
  Serial.begin(115200); // Open connection path to your computer
  Serial.println("\n=== HOME AUTOMATION STARTING ===");

  // Define components as OUTPUT devices
  pinMode(RELAY_PIN,  OUTPUT);
  pinMode(LED_RED,    OUTPUT);
  pinMode(LED_GREEN,  OUTPUT);
  pinMode(LED_BLUE,   OUTPUT);

  digitalWrite(RELAY_PIN, HIGH);  // Ensure Relay defaults to OFF for safety
  setRGB(0,0,0);                 // Turn off RGB light

  oled.begin(); // Boot up OLED screen drivers
  dht.begin();  // Boot up Temperature Sensor drivers

  // RGB Visual Self-Check Animation sequence
  setRGB(1,0,0); delay(200);
  setRGB(0,1,0); delay(200);
  setRGB(0,0,1); delay(200);
  setRGB(0,0,0);

  // Draw Splash/Boot Screen on OLED
  oled.clearBuffer();
  oled.setFont(u8g2_font_8x13_tf);
  oled.drawStr(10, 25, "HOME AUTO");
  oled.drawStr(10, 42, "SYSTEM v2.0");
  oled.setFont(u8g2_font_6x10_tf);
  oled.drawStr(10, 58, "Connecting...");
  oled.sendBuffer();

  // Attempt to link to Wi-Fi and the Blynk Dashboard (This blocks until successful)
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  // Connection Confirmed Screen
  oled.clearBuffer();
  oled.setFont(u8g2_font_8x13_tf);
  oled.drawStr(10, 35, "Connected!");
  oled.sendBuffer();
  delay(1000);

  setRGB(1,0,0);  // Initialization state default: Red LED = Relay OFF

  // STRATEGY: Rather than polling inside the continuous loop, use a timer 
  // to fire 'readSensor' safely every 3000 milliseconds (3 seconds).
  timer.setInterval(3000L, readSensor);

  Serial.println("=== SETUP COMPLETE ===\n");
}

// ── 10. MAIN EXECUTION LOOP (RUNS INFINITELY) ──────────────────────────────
void loop() {
  Blynk.run(); // Keeps communication alive with Blynk Cloud servers
  timer.run(); // Checks schedules and triggers readSensor() precisely every 3s
}