#include <FastLED.h>
#include <NimBLEDevice.h>
#include <HPDL1414.h>

#define NUM_LEDS 1
#define DATA_PIN 4

CRGB leds[NUM_LEDS];

// HPDL1414 pin configuration
#define A0_PIN 19
#define A1_PIN 21
#define WR_PIN 22
#define NUM_DB_PINS 7
#define NUM_ADDR_PINS 2

const byte dbPins[NUM_DB_PINS] = {12, 13, 14, 15, 16, 17, 18}; // Updated DB pins
const byte addrPins[NUM_ADDR_PINS] = {A0_PIN, A1_PIN};         // Updated address pins
const byte otherPin = 0;

HPDL1414 hpdl(dbPins, addrPins, &otherPin, WR_PIN);

float temperature = 0.0;
bool hasReceivedReading = false;
const std::string targetMacAddress = "34:ec:b6:65:de:b2";

NimBLEScan* pBLEScan;
const int scanTime = 5;

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
        displayTemperature((int)round(temperature));
    }
}

void displayTemperature(int temp) {
    hpdl.clear();
    hpdl.print(String(temp) + "C");
}

class MyAdvertisedDeviceCallbacks : public NimBLEAdvertisedDeviceCallbacks {
    void onResult(NimBLEAdvertisedDevice* advertisedDevice) {
        if (advertisedDevice->getAddress().toString() == targetMacAddress) {
            if (advertisedDevice->haveServiceData()) {
                decodeServiceData(advertisedDevice->getServiceData());
            } else {
                Serial.println("No service data available.");
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

    hpdl.print("INIT");
}

float lastTemperature = -999.0;

void loop() {
    pBLEScan->start(scanTime, false);
    if (!hasReceivedReading) {
        leds[0] = CRGB::White;
        FastLED.show();
        hpdl.clear();
        hpdl.print("OFF");
    } else if (temperature != lastTemperature) {
        lastTemperature = temperature;
        displayTemperature((int)round(temperature));
    }
    delay(30000);
}
