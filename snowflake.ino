#include <FastLED.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SparkFun_SCD4x_Arduino_Library.h>
#include <NimBLEDevice.h>

#define OLED_RESET -1
#define NUM_LEDS 6
#define DATA_PIN 4

#define OLED_SDA 21
#define OLED_SCL 22

#define CO2_SDA 17
#define CO2_SCL 16

CRGB leds[NUM_LEDS];
Adafruit_SSD1306 display(128, 64, &Wire, OLED_RESET);
SCD4x scd4x(SCD4x_SENSOR_SCD40);

float temperature = 0.0;
bool hasReceivedReading = false;
const std::string targetMacAddress = "38:1f:8d:97:bd:5d";

NimBLEScan* pBLEScan;
const int scanTime = 5;

void showRainbowEffect(int duration) {
    unsigned long start = millis();
    while (millis() - start < duration) {
        for (int j = 0; j < 256; j++) {  // Cycle through the rainbow colors
            
      //    for (int i = 0; i < NUM_LEDS; i++) {   //Kör regnbåge på alla sex LEDS
            for (int i = 3; i < NUM_LEDS; i++) {  // Kör regnbåge på tre 
                
                leds[i] = CHSV((i * 256 / NUM_LEDS + j) % 256, 255, 255);
            }
            FastLED.show();
            delay(20);  // Delay for smooth transition
        }
    }
}



// Function to compare two strings case-insensitively
bool caseInsensitiveCompare(const char* str1, const char* str2) {
    while (*str1 && *str2) {
        if (tolower(*str1) != tolower(*str2)) {
            return false;
        }
        str1++;
        str2++;
    }
    return *str1 == *str2;
}

// Callback class to handle BLE advertisements
class MyAdvertisedDeviceCallbacks : public NimBLEAdvertisedDeviceCallbacks {
    void onResult(NimBLEAdvertisedDevice* advertisedDevice) {
        if (caseInsensitiveCompare(advertisedDevice->getAddress().toString().c_str(), targetMacAddress.c_str())) {
            if (advertisedDevice->haveServiceData()) {
                decodeServiceData(advertisedDevice->getServiceData());
            } else {
                Serial.println("No service data available.");
            }
        }
    }
};

// CO2 sensor thresholds and variables
int co2High = 1300;
int co2Low = 1200;
uint16_t currentCO2 = 0;
bool isCO2High = false;

// Function prototypes
void displayCenteredText(String text, int textSize, int yPosition);
void setTemperatureLEDColor(float roundedTemperature);
void handleCO2LEDs();
void showRainbowEffect(int duration);
uint16_t decodeLittleEndianU16(uint8_t lowByte, uint8_t highByte);  // Function prototype

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

        hasReceivedReading = true;  // Set flag to true once we receive a valid reading

        // Update display and LED color
        setTemperatureLEDColor(round(temperature));
        displayCenteredText(String((int)round(temperature)), 2, 15);
    }
}

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
void setTemperatureLEDColor(float roundedTemperature) {
    Serial.print("Setting LED color based on temperature: ");
    Serial.println(roundedTemperature);

    if (roundedTemperature >= -50 && roundedTemperature < -5) {
        fill_solid(leds, 3, 0xFF44DD);  // Pink
    } else if (roundedTemperature >= -5 && roundedTemperature < 0) {
        fill_solid(leds, 3, 0x0006FF);  // Blue
    } else if (roundedTemperature >= 0 && roundedTemperature <= 5) {
        fill_solid(leds, 3, 0x00FF06);  // Green
    } else if (roundedTemperature > 5 && roundedTemperature <= 10) {
        fill_solid(leds, 3, 0xFFF600);  // Yellow
    } else if (roundedTemperature > 10 && roundedTemperature <= 15) {
        fill_solid(leds, 3, 0xFFA500);  // Orange
    } else if (roundedTemperature > 15 && roundedTemperature <= 20) {
        fill_solid(leds, 3, 0xFF0000);  // Red
    } else if (roundedTemperature > 20 && roundedTemperature <= 45) {
        fill_solid(leds, 3, 0x800080);  // Purple
    } else {
        fill_solid(leds, 3, 0x000000);  // Off
    }

    FastLED.show();
}

void handleCO2LEDs() {
    if (scd4x.readMeasurement()) {
        uint16_t currentCO2 = scd4x.getCO2();
        Serial.print("CO2 Level: ");
        Serial.println(currentCO2);

        // Show rainbow effect when CO2 > 1300 ppm
        if (currentCO2 >= 1300) {
            showRainbowEffect(5000);
        } 
        // Set LED color based on CO2 level if it's below 1300 ppm
        else if (currentCO2 <= 650) {
            fill_solid(leds + 3, 3, CRGB::Green);  // CO2: <= 650 ppm, Green
        } else if (currentCO2 > 650 && currentCO2 <= 800) {
            fill_solid(leds + 3, 3, CRGB::Yellow); // CO2: 651 - 800 ppm, Yellow
        } else if (currentCO2 > 800 && currentCO2 <= 1000) {
            fill_solid(leds + 3, 3, CRGB::Orange); // CO2: 801 - 1000 ppm, Orange
        } else if (currentCO2 > 1000 && currentCO2 <= 1200) {
            fill_solid(leds + 3, 3, CRGB::Red);    // CO2: 1001 - 1200 ppm, Red
        } else if (currentCO2 > 1200 && currentCO2 <= 1299) {
            fill_solid(leds + 3, 3, CRGB::Purple); // CO2: 1201 - 1299 ppm, Purple
        } else {
            fill_solid(leds + 3, 3, CRGB::White);  // CO2: > 1299 ppm, White (or another default color)
        }

        FastLED.show();
    }
}

void setup() {
    Serial.begin(115200);
    Wire.begin(OLED_SDA, OLED_SCL);
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    display.clearDisplay();
    
    FastLED.addLeds<WS2811, DATA_PIN, RGB>(leds, NUM_LEDS);
    FastLED.setBrightness(200);
    FastLED.setMaxPowerInVoltsAndMilliamps(5, 500);  // Limit power to 5V and 500 mA

    Wire1.begin(CO2_SDA, CO2_SCL);
    if (!scd4x.begin(Wire1)) {
        Serial.println("Failed to initialize SCD4x sensor on custom I2C pins.");
    } else {
        scd4x.stopPeriodicMeasurement();
        scd4x.setSensorAltitude(0); // Adjust altitude if necessary
        scd4x.startPeriodicMeasurement();
    }

    NimBLEDevice::init("");
    pBLEScan = NimBLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks(), true);
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);
    pBLEScan->setActiveScan(true);

    // Display "HEJ" on boot
    displayCenteredText("HEJ", 2, 15);
}

void loop() {
    pBLEScan->start(scanTime, false);
    Serial.println("Scanning...");

    if (!hasReceivedReading) {
        // If we haven't received a valid reading yet, keep LED white and "OFF" on display
        fill_solid(leds, NUM_LEDS, CRGB::White);
        FastLED.show();
        displayCenteredText("OFF", 2, 15);
    } else {
        handleCO2LEDs();  // Handle CO2 LEDs
    }

    delay(30000);  // Scan every 30 seconds
}
