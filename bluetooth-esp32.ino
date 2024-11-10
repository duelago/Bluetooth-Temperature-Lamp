
#include <FastLED.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <NimBLEDevice.h>

#define OLED_RESET -1   // Use -1 for ESP32, no reset pin
#define NUM_LEDS 1
#define DATA_PIN 4

CRGB leds[NUM_LEDS];
Adafruit_SSD1306 display(128, 64, &Wire, OLED_RESET);


float temperature = 0.0;
const std::string targetMacAddress = "34:ec:b6:65:18:3e";  // MAC-adressen till din termometer

// BLE Scan settings
NimBLEScan* pBLEScan;
const int scanTime = 5;  // Scan duration in seconds



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
        displayCenteredText(String((int)round(temperature)), 2, 15);
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
    // Adjusted Y position to start on the third row
    int adjustedYPosition = 32;  // Use 32 or another suitable value based on your needs
    
    display.setCursor(xPosition, adjustedYPosition);
    display.print(text);
    display.display();
}

// Set LED color based on temperature
void setLEDColor(float roundedTemperature) {
    Serial.print("Setting LED color based on temperature: ");
    Serial.println(roundedTemperature);

    if (roundedTemperature >= -50 && roundedTemperature < -5) {
        leds[0] = 0xFF44DD; // Pink
        Serial.println("Shifting color to Pink because temperature is between -50 and -5 °C");
    } else if (roundedTemperature >= -5 && roundedTemperature < 0) {
        leds[0] = 0x0006FF; // Blue
        Serial.println("Shifting color to Blue because temperature is between -5 and 0 °C");
    } else if (roundedTemperature >= 0 && roundedTemperature <= 5) {
        leds[0] = 0x00FF06; // Green
        Serial.println("Shifting color to Green because temperature is between 0 and 5 °C");
    } else if (roundedTemperature > 5 && roundedTemperature <= 10) {
        leds[0] = 0xFFF600; // Yellow
        Serial.println("Shifting color to Yellow because temperature is between 5 and 10 °C");
    } else if (roundedTemperature > 10 && roundedTemperature <= 15) {
        leds[0] = 0xFFA500; // Orange
        Serial.println("Shifting color to Orange because temperature is between 10 and 15 °C");
    } else if (roundedTemperature > 15 && roundedTemperature <= 20) {
        leds[0] = 0xFF0000; // Red
        Serial.println("Shifting color to Red because temperature is between 15 and 20 °C");
    } else if (roundedTemperature > 20 && roundedTemperature <= 45) {
        leds[0] = 0x800080; // Purple
        Serial.println("Shifting color to Purple because temperature is between 20 and 45 °C");
    } else {
        leds[0] = 0x000000; // Off
        Serial.println("Turning off the LED because temperature is outside the defined range");
    }

    FastLED.show();
}

void setup() {
    Serial.begin(115200);
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    display.clearDisplay();
    FastLED.addLeds<WS2811, DATA_PIN, RGB>(leds, NUM_LEDS);
    FastLED.setBrightness(100);

    

    NimBLEDevice::init("");
    pBLEScan = NimBLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks(), true);
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);
    pBLEScan->setActiveScan(true);


    displayCenteredText("HEJ", 2, 15);
}

float lastTemperature = -999.0;  // Initialize with an impossible temperature value

void loop() {
 
    pBLEScan->start(scanTime, false);
    Serial.println("Scanning...");

    // Check if the temperature has changed
    if (temperature != lastTemperature) {
        // Update the display and LED only if the temperature has changed
        lastTemperature = temperature;

        // Update the OLED display and LED
        displayCenteredText(String((int)round(temperature)), 2, 15);
        setLEDColor(round(temperature));
    }

    delay(30000); // Scan every 30 seconds
}
