#include <WiFi.h>
#include <WiFiManager.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <HPDL1414.h>

// HPDL1414 pin configuration
#define A0_PIN 22
#define A1_PIN 23
#define WR_PIN 15
#define NUM_DB_PINS 7
#define NUM_ADDR_PINS 2

const byte dbPins[NUM_DB_PINS] = {13, 12, 14, 27, 26, 25, 33}; // Updated DB pins
const byte addrPins[NUM_ADDR_PINS] = {A0_PIN, A1_PIN};         // Updated address pins
const byte wrenPins[] = {WR_PIN};
HPDL1414 hpdl(dbPins, addrPins, wrenPins, sizeof(wrenPins));

// NTP client setup
WiFiUDP udp;
NTPClient timeClient(udp, "pool.ntp.org", 3600, 60000);  // Swedish time zone (UTC +1)

// Variables to store time
int lastSecond = -1;
unsigned long lastUpdate = 0;

void setup() {
  // Initialize serial for debugging
  Serial.begin(115200);

  // Connect to WiFi using WiFiManager
  WiFiManager wifiManager;
  wifiManager.autoConnect("ESP32_HPDL1414"); // Use default SSID name

  // Start NTP client
  timeClient.begin();
  
  // Initialize HPDL1414 display
  hpdl.begin();
}

void loop() {
  // Update time every 30 seconds
  if (millis() - lastUpdate >= 30000) {
    lastUpdate = millis();
    updateDisplay();
  }

  // Non-blocking NTP time update
  timeClient.update();
}

void updateDisplay() {
  // Get current time from NTP client
  int hours = timeClient.getHours();
  int minutes = timeClient.getMinutes();

  // Format hours and minutes (hours in 12-hour format)
  int displayHours = hours % 12;
  if (displayHours == 0) {
    displayHours = 12;  // Show 12 instead of 0
  }

  // Split minutes into tens and ones
  int tensOfMinutes = minutes / 10;
  int onesOfMinutes = minutes % 10;

  // Clear previous display
  hpdl.clear();

  // Display the hours (position 1) and tens of minutes (position 3)
  hpdl.setCursor(0);      // Position 1 (hours)
  hpdl.write(displayHours + '0'); // Convert to ASCII
  hpdl.setCursor(2);      // Position 3 (tens of minutes)
  hpdl.write(tensOfMinutes + '0'); // Convert to ASCII
  
  // Display the colon in position 2
  hpdl.setCursor(1);      // Position 2 (colon)
  hpdl.write(':');

  // Display the ones of minutes (position 4)
  hpdl.setCursor(3);      // Position 4 (ones of minutes)
  hpdl.write(onesOfMinutes + '0'); // Convert to ASCII


}
