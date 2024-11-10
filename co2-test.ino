#include <FastLED.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <NimBLEDevice.h>
#include <SparkFun_SCD4x_Arduino_Library.h>

#define OLED_RESET -1   // Use -1 for ESP32, no reset pin
#define NUM_LEDS 1
#define DATA_PIN 4

CRGB leds[NUM_LEDS];
Adafruit_SSD1306 display(128, 64, &Wire, OLED_RESET);
SCD4x scd4x(SCD4x_SENSOR_SCD40);

float temperature = 0.0;
const std::string targetMacAddress = "34:ec:b6:65:18:3e";  // MAC address of your thermometer

// BLE Scan settings
NimBLEScan* pBLEScan;
const int scanTime = 5;  // Scan duration in seconds

// CO2 thresholds
const int co2High = 1300;
const int co2Low = 1200;
uint16_t currentCO2 = 0;
bool isCO2High = false;

// Helper function to create a rainbow effect
void rainbowEffect() {
    const int numSteps = 255;
    for (int i = 0; i < numSteps; i++) {
        fill_rainbow(leds, NUM_LEDS, i, 7);
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
    int adjustedYPosition = 32;  // Adjust this value as needed
    
    display.setCursor(xPosition, adjustedYPosition);
    display.print(text);
    display.display();
}

// Set LED color based on temperature
void setLEDColor(float roundedTemperature) {
    if (!isCO2High) {  // Only set color if CO2 levels are normal
        if (roundedTemperature >= -50 && roundedTemperature < -5) {
            leds[0] = 0xFF44DD; // Pink
        } else if (roundedTemperature >= -5 && roundedTemperature < 0) {
            leds[0] = 0x0006FF; // Blue
        } else if (roundedTemperature >= 0 && roundedTemperature <= 5) {
            leds[0] = 0x00FF06; // Green
        } else if (roundedTemperature > 5 && roundedTemperature <= 10) {
            leds[0] = 0xFFF600; // Yellow
        } else if (roundedTemperature > 10 && roundedTemperature <= 15) {
            leds[0] = 0xFFA500; // Orange
        } else if (roundedTemperature > 15 && roundedTemperature <= 20) {
            leds[0] = 0xFF0000; // Red
        } else if (roundedTemperature > 20 && roundedTemperature <= 45) {
            leds[0] = 0x800080; // Purple
        } else {
            leds[0] = 0x000000; // Off
        }
        FastLED.show();
    }
}

void setup() {
    Serial.begin(115200);

    // Initiera Wire med alternativa I2C-pinnar
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
        setLEDColor(round(temperature));
    }

    // Handle CO2 measurement
    if (scd4x.readMeasurement()) {
        currentCO2 = scd4x.getCO2();
        Serial.print("CO2: ");
        Serial.println(currentCO2);

        if (currentCO2 > co2High) {
            if (!isCO2High) {  // Only trigger if state changes
                isCO2High = true;
                rainbowEffect();
            }
        } else if (currentCO2 <= co2Low) {
            if (isCO2High) {  // Only revert if state changes
                isCO2High = false;
                setLEDColor(round(temperature));  // Return to normal mode
            }
        }
    }

    delay(30000);  // Loop delay to reduce scan frequency
}
