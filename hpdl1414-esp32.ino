#include <FastLED.h>
#include <NimBLEDevice.h>
#include <HPDL1414.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#define NUM_LEDS 1
#define DATA_PIN 4

CRGB leds[NUM_LEDS];

// HPDL1414 pin configuration
#define A0_PIN 22
#define A1_PIN 23
#define WR_PIN 15
#define NUM_DB_PINS 7
#define NUM_ADDR_PINS 2

const byte dbPins[NUM_DB_PINS] = {13, 12, 14, 27, 26, 25, 33};
const byte addrPins[NUM_ADDR_PINS] = {A0_PIN, A1_PIN};
const byte wrenPins[] = {WR_PIN};
HPDL1414 hpdl(dbPins, addrPins, wrenPins, sizeof(wrenPins));

float temperature = 0.0;
bool hasReceivedReading = false;
const std::string targetMacAddress = "34:ec:b6:65:de:b2";

// NTP client setup
WiFiUDP udp;
NTPClient timeClient(udp, "pool.ntp.org", 0, 60000);  // UTC

// Display state variables
enum DisplayMode { SHOW_TIME, SHOW_TEMP };
DisplayMode currentMode = SHOW_TIME;
unsigned long lastModeChange = 0;
const unsigned long modeDuration = 10000; // 10 seconds per mode

// Variables to store time
int lastSecond = -1;
unsigned long lastUpdate = 0;

NimBLEScan* pBLEScan;
const int scanTime = 5;

// Function to check if DST is in effect in Sweden
bool isDST(int year, int month, int day, int hour) {
    if (month < 3 || month > 10) return false;
    if (month > 3 && month < 10) return true;
    
    int lastSunday;
    if (month == 3) {
        lastSunday = 31;
        while (true) {
            struct tm tm = {0, 0, 0, lastSunday, 2, year - 1900};
            mktime(&tm);
            if (tm.tm_wday == 0) break;
            lastSunday--;
        }
        if (day > lastSunday) return true;
        if (day == lastSunday && hour >= 1) return true;
    } else {
        lastSunday = 31;
        while (true) {
            struct tm tm = {0, 0, 0, lastSunday, 9, year - 1900};
            mktime(&tm);
            if (tm.tm_wday == 0) break;
            lastSunday--;
        }
        if (day < lastSunday) return true;
        if (day == lastSunday && hour < 1) return true;
    }
    
    return false;
}

uint16_t decodeLittleEndianU16(uint8_t lowByte, uint8_t highByte) {
    return (uint16_t)((highByte << 8) | lowByte);
}

void decodeServiceData(const std::string& payload) {
    if (payload.length() >= 14 && payload[0] == 0x40) {
        int16_t temp_int = (payload[6] & 0xff) | ((payload[7] & 0xff) << 8);
        temperature = temp_int / 100.0;
        Serial.print("Temperature: ");
        Serial.print(temperature);
        Serial.println(" °C");

        hasReceivedReading = true;
        setLEDColor(round(temperature));
    }
}

void displayTemperature(int temp) {
    hpdl.clear();
    String tempStr = String(temp);
    
    if (tempStr.length() == 1) {
        hpdl.print("   ");
        hpdl.print(tempStr);
    } else if (tempStr.length() == 2) {
        hpdl.print("  ");
        hpdl.print(tempStr);
    } else if (tempStr.length() == 3) {
        hpdl.print(" ");
        hpdl.print(tempStr);
    } else {
        hpdl.print(temp);
    }
}

void updateTimeDisplay() {
    timeClient.update();
    time_t epochTime = timeClient.getEpochTime();
    struct tm *ptm = gmtime(&epochTime);
    
    bool dst = isDST(ptm->tm_year + 1900, ptm->tm_mon + 1, ptm->tm_mday, ptm->tm_hour);
    int hour = ptm->tm_hour + 1 + (dst ? 1 : 0);
    if (hour >= 24) hour -= 24;
    
    int minutes = ptm->tm_min;
    int displayHours = hour % 12;
    if (displayHours == 0) displayHours = 12;

    hpdl.clear();
    hpdl.setCursor(0);
    if (displayHours >= 10) {
        hpdl.write((displayHours / 10) + '0');
        hpdl.setCursor(1);
        hpdl.write((displayHours % 10) + '0');
        hpdl.setCursor(2);
    } else {
        hpdl.write(displayHours + '0');
        hpdl.setCursor(1);
        hpdl.write(':');
        hpdl.setCursor(2);
    }

    hpdl.write((minutes / 10) + '0');
    hpdl.setCursor(3);
    hpdl.write((minutes % 10) + '0');
}

class MyAdvertisedDeviceCallbacks : public NimBLEAdvertisedDeviceCallbacks {
    void onResult(NimBLEAdvertisedDevice* advertisedDevice) {
        if (advertisedDevice->getAddress().toString() == targetMacAddress) {
            if (advertisedDevice->haveServiceData()) {
                decodeServiceData(advertisedDevice->getServiceData());
            }
        }
    }
};

void setLEDColor(float roundedTemperature) {
    if (roundedTemperature < 0) {
        leds[0] = CRGB::Blue;
    } else if (roundedTemperature <= 10) {
        leds[0] = CRGB::Green;
    } else if (roundedTemperature <= 20) {
        leds[0] = CRGB::Yellow;
    } else {
        leds[0] = CRGB::Red;
    }
    FastLED.show();
}

void setup() {
    Serial.begin(115200);
    WiFiManager wifiManager;
    wifiManager.autoConnect("Retrolampan"); 

    hpdl.begin();
    hpdl.clear();
    FastLED.addLeds<WS2811, DATA_PIN, RGB>(leds, NUM_LEDS);
    FastLED.setBrightness(200);
    leds[0] = CRGB::White;
    FastLED.show();

    NimBLEDevice::init("");
    pBLEScan = NimBLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks(), true);
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);
    pBLEScan->setActiveScan(true);

    hpdl.print("TEMP");
    lastModeChange = millis();
}

void loop() {
    // Check if it's time to switch display mode
    if (millis() - lastModeChange >= modeDuration) {
        currentMode = (currentMode == SHOW_TIME) ? SHOW_TEMP : SHOW_TIME;
        lastModeChange = millis();
    }

    // Update the display based on current mode
    if (currentMode == SHOW_TIME) {
        updateTimeDisplay();
    } else if (hasReceivedReading) {
        displayTemperature((int)round(temperature));
    } else {
        hpdl.clear();
        hpdl.print("OFF");
    }

    // Run BLE scan every 5 seconds
    if (millis() % 5000 < 100) {  // Roughly every 5 seconds
        pBLEScan->start(scanTime, false);
    }

    delay(100);  // Small delay to prevent busy-waiting
}
