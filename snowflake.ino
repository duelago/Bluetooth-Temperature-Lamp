#include <FastLED.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <NimBLEDevice.h>

#define OLED_RESET -1   // Use -1 for ESP32, no reset pin
#define NUM_LEDS 6      // Updated to use 6 LEDs
#define DATA_PIN 4

CRGB leds[NUM_LEDS];
Adafruit_SSD1306 display(128, 64, &Wire, OLED_RESET);

float temperature = 0.0;
bool hasReceivedReading = false;  // Flag to indicate if a valid reading has been received
const std::string targetMacAddress = "38:1f:8d:97:bd:5d";  // MAC address of your thermometer

// BLE Scan settings
NimBLEScan* pBLEScan;
const int scanTime = 5;  // Scan duration in seconds

// Helper function to decode a little-endian 16-bit unsigned integer
uint16_t decodeLittleEndianU16(uint8_t lowByte, uint8_t highByte) {
    return (uint16_t)((highByte << 8) | lowByte);
}

// Function to convert a char to lowercase
char toLowerChar(char c) {
    if (c >= 'A' && c <= 'Z') {
        return c + 32;
    }
    return c;
}

// Function to convert a string to lowercase
String toLowerCase(const String& str) {
    String lowerStr = str;
    for (int i = 0; i < lowerStr.length(); i++) {
        lowerStr[i] = toLowerChar(lowerStr[i]);
    }
    return lowerStr;
}

// Function to decode the raw service data and update the temperature
void decodeServiceData(const std::string& payload) {
    if (payload.length() >= 14 && payload[0] == 0x40) {
        int16_t temp_int = (payload[6] & 0xff) | ((payload[7] & 0xff) << 8);
        temperature = temp_int / 100.0;
        Serial.print("Temperature: ");
        Serial.print(temperature);
        Serial.println(" °C");

        hasReceivedReading = true;  // Set flag to true once we receive a valid reading

        // Update display and LED color
        setLEDColor(round(temperature));
        displayCenteredText(String((int)round(temperature)), 2, 15);
    }
}

// Callback to handle BLE Advertisements
class MyAdvertisedDeviceCallbacks : public NimBLEAdvertisedDeviceCallbacks {
    void onResult(NimBLEAdvertisedDevice* advertisedDevice) {
        if (toLowerCase(advertisedDevice->getAddress().toString().c_str()) == toLowerCase(targetMacAddress.c_str())) {
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
    int adjustedYPosition = 32;  // Adjusted Y position to start on the third row

    display.setCursor(xPosition, adjustedYPosition);
    display.print(text);
    display.display();
}

// Set LED color based on temperature
void setLEDColor(float roundedTemperature) {
    Serial.print("Setting LED color based on temperature: ");
    Serial.println(roundedTemperature);

    if (roundedTemperature >= -50 && roundedTemperature < -5) {
        fill_solid(leds, NUM_LEDS, 0xFF44DD);  // Pink
    } else if (roundedTemperature >= -5 && roundedTemperature < 0) {
        fill_solid(leds, NUM_LEDS, 0x0006FF);  // Blue
    } else if (roundedTemperature >= 0 && roundedTemperature <= 5) {
        fill_solid(leds, NUM_LEDS, 0x00FF06);  // Green
    } else if (roundedTemperature > 5 && roundedTemperature <= 10) {
        fill_solid(leds, NUM_LEDS, 0xFFF600);  // Yellow
    } else if (roundedTemperature > 10 && roundedTemperature <= 15) {
        fill_solid(leds, NUM_LEDS, 0xFFA500);  // Orange
    } else if (roundedTemperature > 15 && roundedTemperature <= 20) {
        fill_solid(leds, NUM_LEDS, 0xFF0000);  // Red
    } else if (roundedTemperature > 20 && roundedTemperature <= 45) {
        fill_solid(leds, NUM_LEDS, 0x800080);  // Purple
    } else {
        fill_solid(leds, NUM_LEDS, 0x000000);  // Off
    }

    FastLED.show();
}

void setup() {
    Serial.begin(115200);
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    display.clearDisplay();
    
    FastLED.addLeds<WS2811, DATA_PIN, RGB>(leds, NUM_LEDS);
    FastLED.setBrightness(200);
    FastLED.setMaxPowerInVoltsAndMilliamps(5, 500);  // Limit power to 5V and 500 mA

    // Initial LED color set to white before any valid temperature reading
    fill_solid(leds, NUM_LEDS, CRGB::White);
    FastLED.show();

    NimBLEDevice::init("");
    pBLEScan = NimBLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks(), true);
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);
    pBLEScan->setActiveScan(true);

    // Display "HEJ" on boot
    displayCenteredText("HEJ", 2, 15);
}

float lastTemperature = -999.0;  // Initialize with an impossible temperature value

void loop() {
    pBLEScan->start(scanTime, false);
    Serial.println("Scanning...");

    if (!hasReceivedReading) {
        // If we haven't received a valid reading yet, keep LED white and "OFF" on display
        fill_solid(leds, NUM_LEDS, CRGB::White);
        FastLED.show();
        displayCenteredText("OFF", 2, 15);
    } else if (temperature != lastTemperature) {
        // Update the display and LED only if the temperature has changed
        lastTemperature = temperature;
        displayCenteredText(String((int)round(temperature)), 2, 15);
        setLEDColor(round(temperature));
    }

    delay(30000);  // Scan every 30 seconds
}
