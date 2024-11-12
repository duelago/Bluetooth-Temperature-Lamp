#include <FastLED.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <NimBLEDevice.h>
#include <SparkFun_SCD4x_Arduino_Library.h>

#define OLED_RESET -1   // Use -1 for ESP32, no reset pin
#define NUM_LEDS 6      // Updated to 6 LEDs
#define DATA_PIN 4

CRGB leds[NUM_LEDS];
Adafruit_SSD1306 display(128, 64, &Wire, OLED_RESET);
SCD4x scd4x(SCD4x_SENSOR_SCD40);

float temperature = 0.0;
const std::string targetMacAddress = "fc:6a:62:d8:23:f4";  // MAC address of your thermometer

// BLE Scan settings
NimBLEScan* pBLEScan;
const int scanTime = 5;  // Scan duration in seconds

// CO2 thresholds
const int co2High = 1300;
const int co2Low = 1250;
uint16_t currentCO2 = 0;
bool isCO2High = false;

// Helper function to create a rainbow effect on LEDs 4, 5, 6
void rainbowEffect() {
    const int numSteps = 255;
    for (int i = 0; i < numSteps; i++) {
        fill_rainbow(&leds[3], 3, i, 7);  // Apply rainbow effect to LEDs 4, 5, 6
        FastLED.show();
        delay(10);
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
    int adjustedYPosition = yPosition;  // Use provided yPosition
    
    display.setCursor(xPosition, adjustedYPosition);
    display.print(text);
    display.display();
}

// Set LED color based on temperature for LEDs 1, 2, 3
void setTemperatureLEDs(float roundedTemperature) {
    CRGB color;
    if (roundedTemperature >= -50 && roundedTemperature < -5) {
        color = 0xFF44DD; // Pink
    } else if (roundedTemperature >= -5 && roundedTemperature < 0) {
        color = 0x0006FF; // Blue
    } else if (roundedTemperature >= 0 && roundedTemperature <= 5) {
        color = 0x00FF06; // Green
    } else if (roundedTemperature > 5 && roundedTemperature <= 10) {
        color = 0xFFF600; // Yellow
    } else if (roundedTemperature > 10 && roundedTemperature <= 15) {
        color = 0xFFA500; // Orange
    } else if (roundedTemperature > 15 && roundedTemperature <= 20) {
        color = 0xFF0000; // Red
    } else if (roundedTemperature > 20 && roundedTemperature <= 45) {
        color = 0x800080; // Purple
    } else {
        color = 0x000000; // Off
    }

    leds[0] = leds[1] = leds[2] = color; // Set LEDs 1, 2, 3 to the same color
    FastLED.show();
}

// Set LED color based on CO2 level for LEDs 4, 5, 6
void setCO2LEDs(uint16_t co2Level) {
    CRGB color;
    if (co2Level < 500) {
        color = CRGB::Green;
    } else if (co2Level < 700) {
        color = CRGB::Yellow;
    } else if (co2Level < 900) {
        color = CRGB::Red;
    } else if (co2Level < 1300) {
        color = CRGB::Purple;
    } else {
        rainbowEffect(); // Trigger rainbow effect if CO2 is above 1300
        return;
    }

    leds[3] = leds[4] = leds[5] = color; // Set LEDs 4, 5, 6 to the same color
    FastLED.show();
}

void setup() {
    Serial.begin(115200);

    Wire.begin(18, 19); // SDA = GPIO 18, SCL = GPIO 19

    if (!scd4x.begin(Wire)) {
        Serial.println("Failed to initialize SCD40 sensor.");
    } else {
        Serial.println("SCD40 sensor initialized successfully.");
    }
    
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    display.clearDisplay();
    FastLED.addLeds<WS2811, DATA_PIN, RGB>(leds, NUM_LEDS);
    FastLED.setBrightness(200);

    NimBLEDevice::init("");
    pBLEScan = NimBLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks(), true);
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);
    pBLEScan->setActiveScan(true);

    displayCenteredText("HEJ", 2, 15);

    // Initialize SCD4x sensor
    if (!scd4x.begin(Wire)) {
        Serial.println("Failed to initialize SCD4x sensor. Continuing with temperature measurements only.");
        isCO2High = false;
    } else {
        scd4x.stopPeriodicMeasurement();
        scd4x.setSensorAltitude(0);  // Set altitude in meters
        scd4x.setAutomaticSelfCalibrationEnabled(false);  // Disable auto-calibration
        scd4x.startPeriodicMeasurement();  // Start periodic CO2 measurement
    }
}

float lastTemperature = -999.0;  // Initialize with an impossible temperature value

void loop() {
    pBLEScan->start(scanTime, false);
    Serial.println("Scanning...");

    // Check if the temperature has changed
    if (temperature != lastTemperature) {
        lastTemperature = temperature;
        displayCenteredText(String((int)round(temperature)), 2, 15);
        setTemperatureLEDs(round(temperature));
    }

    // Handle CO2 measurement
    if (scd4x.readMeasurement()) {
        currentCO2 = scd4x.getCO2();
        Serial.print("CO2: ");
        Serial.println(currentCO2);

        setCO2LEDs(currentCO2);

        if (currentCO2 > co2High) {
            if (!isCO2High) {  // Only trigger if state changes
                isCO2High = true;
                displayCenteredText(String(currentCO2) + " ppm", 1, 0);
                rainbowEffect();
            }
        } else if (currentCO2 <= co2Low) {
            if (isCO2High) {  // Only revert if state changes
                isCO2High = false;
                setTemperatureLEDs(round(temperature));  // Return to temperature-based colors
            }
        }
    }

    delay(30000);  // Loop delay to reduce scan frequency
}
