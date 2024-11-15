#include <FastLED.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SparkFun_SCD4x_Arduino_Library.h>
#include <NimBLEDevice.h>
#include <WiFiManager.h>
#include <WebServer.h>
#include <ElegantOTA.h>
#include <HTTPClient.h>  
#include <ArduinoJson.h> 
#include <EEPROM.h>  

#define SONG_TITLE_ADDRESS 0


#define OLED_RESET -1
#define NUM_LEDS 6
#define DATA_PIN 4

#define OLED_SDA 21
#define OLED_SCL 22

#define CO2_SDA 17
#define CO2_SCL 16

WebServer server(80);

CRGB leds[NUM_LEDS];
Adafruit_SSD1306 display(128, 64, &Wire, OLED_RESET);
SCD4x scd4x(SCD4x_SENSOR_SCD40);

const char* apiUrl = "https://listenapi.planetradio.co.uk/api9.2/nowplaying/mme";

unsigned long previousMillis = 0; // Stores the last time BLE scan was triggered
const unsigned long scanInterval = 10000; // Time between BLE scans (e.g., 10 seconds)
unsigned long previousTitleCheckMillis = 0; // Stores the last time track title was checked
const unsigned long titleCheckInterval = 45000; // 45 seconds between track title checks


float temperature = 0.0;
bool hasReceivedReading = false;
const std::string targetMacAddress = "38:1f:8d:97:bd:5d";

NimBLEScan* pBLEScan;
const int scanTime = 5;

void showRainbowEffect(int duration) {
    unsigned long start = millis();
    while (millis() - start < duration) {
        for (int j = 0; j < 256; j++) {  // Cycle through the rainbow colors
            for (int i = 3; i < NUM_LEDS; i++) {  // Only apply to the last three LEDs
                leds[i] = CHSV((i * 256 / NUM_LEDS + j) % 256, 255, 255);
            }
            FastLED.show();
            delay(5);  // Delay for smooth transition
        }
    }
}

// Function to write the song title to EEPROM
void writeSongTitleToEEPROM(const String& songTitle) {
    int len = songTitle.length();
    for (int i = 0; i < len; i++) {
        EEPROM.write(SONG_TITLE_ADDRESS + i, songTitle[i]);
    }
    EEPROM.write(SONG_TITLE_ADDRESS + len, '\0');  // Null-terminate the string
    EEPROM.commit();  // Save changes to EEPROM
}

// Function to read the song title from EEPROM
String readSongTitleFromEEPROM() {
    String songTitle = "";
    for (int i = SONG_TITLE_ADDRESS; i < SONG_TITLE_ADDRESS + 32; i++) {
        char c = EEPROM.read(i);
        if (c == '\0') break;
        songTitle += c;
    }
    return songTitle;
}

// Function to handle the form for setting the song title
void handleSetSongTitle() {
    String songTitle = server.arg("songTitle");  // Get the song title from the form
    if (songTitle.length() > 0) {
        writeSongTitleToEEPROM(songTitle);  // Store it in EEPROM
    }
    server.send(200, "text/html", "<html><body><h1>Song Title Saved!</h1><a href='/'>Back</a></body></html>");
}

// Function to make HTTP request and parse JSON
String getSongTitle() {
    HTTPClient http;
    http.begin(apiUrl);
    int httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
        String payload = http.getString();
        Serial.println("HTTP Response: " + payload);

        // Parse JSON
        StaticJsonDocument<1024> doc;
        DeserializationError error = deserializeJson(doc, payload);

        if (error) {
            Serial.print("JSON Deserialization failed: ");
            Serial.println(error.f_str());
            return "Error parsing JSON";
        }

        // Extract the song title from the JSON response
        // Adjusted to match the correct JSON structure
        const char* songTitle = doc["TrackTitle"];
        return songTitle ? String(songTitle) : "No song data";
    } else {
        Serial.print("HTTP GET request failed, error: ");
        Serial.println(http.errorToString(httpResponseCode).c_str());
        return "HTTP error";
    }
    http.end();
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
void blinkRedLEDs() {
    fill_solid(leds, NUM_LEDS, CRGB::Red); // Turn all LEDs red
    FastLED.show();
    delay(500); // Delay for 500ms
    fill_solid(leds, NUM_LEDS, CRGB::Black); // Turn off LEDs
    FastLED.show();
    delay(500); // Delay for 500ms to complete the blink cycle
}

void handleRoot() {
    String currentSongTitle = getSongTitle();  // Get the current song title from the API
    String storedSongTitle = readSongTitleFromEEPROM();  // Get the song title from EEPROM

    // HTML content with both current and stored song titles
    String htmlContent = "<html><head><meta charset=\"UTF-8\"><title>Now Playing</title></head><body>"
                         "<h1>Now Playing:</h1>"
                         "<p><strong>Current Song:</strong> " + currentSongTitle + "</p>"  // Show current song
                         "<h2>Stored Song Title:</h2>"
                         "<p>" + storedSongTitle + "</p>"  // Show stored song title from EEPROM
                         "<form action='/setSongTitle' method='POST'>"
                         "<label for='songTitle'>Enter Song Title: </label>"
                         "<input type='text' id='songTitle' name='songTitle' required>"
                         "<input type='submit' value='Save'>"
                         "</form>"
                         "<p><a href=\"/update\">Firmware update</a></p>"
                         "</body></html>";

    server.send(200, "text/html", htmlContent);  // Send the HTML response
}


void setup() {
    Serial.begin(115200);
    WiFiManager wifiManager;
    wifiManager.autoConnect("Snowflake");
    Wire.begin(OLED_SDA, OLED_SCL);
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    display.clearDisplay();
    
    FastLED.addLeds<WS2811, DATA_PIN, RGB>(leds, NUM_LEDS);
    FastLED.setBrightness(200);
    FastLED.setMaxPowerInVoltsAndMilliamps(5, 500);  // Limit power to 5V and 500 mA

    EEPROM.begin(512);


    server.on("/", HTTP_GET, handleRoot);  // Handle GET requests to '/'
    server.on("/setSongTitle", HTTP_POST, handleSetSongTitle);  // Handle POST requests to '/setSongTitle'



    ElegantOTA.begin(&server);
    server.begin();

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


    // Regular track title check every 45 seconds (as in the original code)
    unsigned long currentMillis = millis();
    if (currentMillis - previousTitleCheckMillis >= titleCheckInterval) {
        previousTitleCheckMillis = currentMillis;

        String storedSongTitle = readSongTitleFromEEPROM();  // Get the stored song title
        String currentSongTitle = getSongTitle();  // Get the current track title from API

        // If the stored song title matches the current track title, do something
        if (caseInsensitiveCompare(storedSongTitle.c_str(), currentSongTitle.c_str())) {
            Serial.println("Playing stored song: " + currentSongTitle);
            blinkRedLEDs(); // Add your logic to trigger actions (e.g., show on display, change LED color, etc.)
        }
    }

    // Existing BLE and CO2 handling code
    if (currentMillis - previousMillis >= scanInterval) {
        previousMillis = currentMillis;
        pBLEScan->start(scanTime, false);
        Serial.println("Scanning...");
    }

    if (!hasReceivedReading) {
        fill_solid(leds, NUM_LEDS, CRGB::White);
        FastLED.show();
        displayCenteredText("OFF", 2, 15);
    } else {
        handleCO2LEDs();
    }

    ElegantOTA.loop();
    server.handleClient();
}
