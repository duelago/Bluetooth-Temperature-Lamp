#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <FastLED.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFiManager.h>
#include <ESPmDNS.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ElegantOTA.h>
#include <NimBLEDevice.h>

char ssid[] = "";       // Leave empty to configure with WiFi Manager
char password[] = "";   // Leave empty to configure with WiFi Manager

#define OLED_RESET -1   // Use -1 for ESP32, no reset pin
#define NUM_LEDS 1
#define DATA_PIN 2

CRGB leds[NUM_LEDS];
Adafruit_SSD1306 display(128, 64, &Wire, OLED_RESET);

WebServer server(80);
float temperature = 0.0;
const std::string targetMacAddress = "34:ec:b6:65:18:3e";  // Your target device MAC address

// BLE Scan settings
NimBLEScan* pBLEScan;
const int scanTime = 5;  // Scan duration in seconds

// HTML template with placeholder for temperature
const char* html = R"(
<!DOCTYPE html>
<html>
<head>
    <title>Temperature Lamp</title>
    <style>
        body { text-align: center; font-family: Arial, sans-serif; }
        h1 { color: #4CAF50; }
        p { font-size: 20px; }
    </style>
</head>
<body>
    <h1>Temperature Lamp</h1>
    <p>Current Temperature: -- °C</p>
</body>
</html>
)";

// Helper function to decode a little-endian 16-bit unsigned integer
uint16_t decodeLittleEndianU16(uint8_t lowByte, uint8_t highByte) {
    return (uint16_t)((highByte << 8) | lowByte);
}

// Function to decode the raw service data and update the temperature
void decodeServiceData(const std::string& payload) {
    if (payload.length() >= 14 && payload[0] == 0x40) {
        int16_t temp_int = (payload[6] & 0xff) | ((payload[7] & 0xff) << 8);
        temperature = temp_int / 100.0;
        Serial.print("Temperature: ");
        Serial.print(temperature);
        Serial.println(" °C");

        // Update display and LED color
        setLEDColor(round(temperature));
        displayCenteredText(String(round(temperature)), 2, 15);
    }
}

// Callback to handle BLE Advertisements
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

// Function to display centered text on the OLED screen
void displayCenteredText(String text, int textSize, int yPosition) {
    display.clearDisplay();
    display.setTextSize(textSize);
    display.setTextColor(WHITE);
    int16_t x1, y1;
    uint16_t width, height;
    display.getTextBounds(text, 0, 0, &x1, &y1, &width, &height);
    int xPosition = (display.width() - width) / 2;
    display.setCursor(xPosition, yPosition);
    display.print(text);
    display.display();
}

// Set LED color based on temperature
void setLEDColor(float roundedTemperature) {
    if (roundedTemperature >= 0 && roundedTemperature <= 5) {
        leds[0] = 0x00FF06; // Green
    } else if (roundedTemperature >= -5 && roundedTemperature < 0) {
        leds[0] = 0x0006FF; // Blue
    } else if (roundedTemperature >= -50 && roundedTemperature < -5) {
        leds[0] = 0xFF44DD; // Pink
    } else if (roundedTemperature >= 6 && roundedTemperature <= 10) {
        leds[0] = 0xFFF600; // Yellow
    } else if (roundedTemperature >= 11 && roundedTemperature <= 15) {
        leds[0] = 0xFFA500; // Orange
    } else if (roundedTemperature >= 16 && roundedTemperature <= 20) {
        leds[0] = 0xFF0000; // Red
    } else if (roundedTemperature >= 21 && roundedTemperature <= 45) {
        leds[0] = 0x800080; // Purple
    }
    FastLED.show();
}

void setup() {
    Serial.begin(115200);
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    display.clearDisplay();
    FastLED.addLeds<WS2811, DATA_PIN, RGB>(leds, NUM_LEDS);
    FastLED.setBrightness(100);

    WiFiManager wifiManager;
    wifiManager.autoConnect("Temperaturlampan");
    ElegantOTA.begin(&server);

    if (!MDNS.begin("lampan")) {
        Serial.println("Error starting mDNS");
    }

    NimBLEDevice::init("");
    pBLEScan = NimBLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks(), true);
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);
    pBLEScan->setActiveScan(true);

    server.on("/", HTTP_GET, []() {
        // Replace the placeholder "--" in the HTML with the current temperature
        String htmlWithTemp = String(html);
        htmlWithTemp.replace("--", String(temperature));
        server.send(200, "text/html", htmlWithTemp);  // Send the HTML page with the temperature
    });

    server.begin();
    displayCenteredText("Starting...", 2, 15);
}

void loop() {
    ElegantOTA.loop();
    server.handleClient();
    pBLEScan->start(scanTime, false);
    Serial.println("Scanning...");
    delay(30000); // Scan every 30 seconds
}
