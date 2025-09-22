#include <FastLED.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <NimBLEDevice.h>
#include <WiFiManager.h>
#include <WebServer.h>
#include <ElegantOTA.h>
#include <HTTPClient.h>  
#include <ArduinoJson.h> 
#include <EEPROM.h>  

#define SONG_TITLE_ADDRESS 0
#define DRY_VALUE_ADDRESS 50
#define WET_VALUE_ADDRESS 54

// OLED Display definitions  
#define SCREEN_WIDTH 64
#define SCREEN_HEIGHT 48
#define OLED_RESET -1

#define NUM_LEDS 6
#define DATA_PIN 4

#define OLED_SDA 21
#define OLED_SCL 22

#define MOISTURE_PIN 32

WebServer server(80);

CRGB leds[NUM_LEDS];
// Fixed display declaration for older library
Adafruit_SSD1306 display(OLED_RESET);

// Set the interval for all sensor checks to 45 seconds (45,000 milliseconds)
unsigned long previousMoistureCheckMillis = 0;
unsigned long previousTitleCheckMillis = 0;
unsigned long previousWindUpdateMillis = 0; // Add a variable to track wind update time
const unsigned long sensorCheckInterval = 45000; // 45 seconds (45,000 milliseconds)

const char* apiUrl = "https://listenapi.planetradio.co.uk/api9.2/nowplaying/mme";

unsigned long previousBLEMillis = 0; // Stores the last time BLE scan was triggered
const unsigned long scanInterval = 10000; // Time between BLE scans (e.g., 10 seconds)

const char* apiHost = "api.holfuy.com";

// The updated interval for all checks
const unsigned long updateInterval = 45000;  // 45 seconds in milliseconds

float temperature = 0.0;
float windSpeed = 0.0;
bool hasReceivedReading = false;
const std::string targetMacAddress = "38:1f:8d:97:bd:5d";

//bool blockLEDUpdates = false; // Flag to block other LED updates
// Global flag to control LED behavior
bool isBlinking = false;

// Moisture sensor variables
int moistureValue = 0;
int dryValue = 3000;  // Default dry calibration value
int wetValue = 1000;  // Default wet calibration value

NimBLEScan* pBLEScan;
const int scanTime = 5;

// Function prototypes
void displayCenteredText(String text, int textSize, int yPosition);
void setTemperatureLEDColor(float roundedTemperature);
void setWindSpeedLEDColor(float windSpeed);
void handleMoistureLEDs();
void blinkRedGreen(int duration);
void blinkRedLEDs();
void decodeServiceData(const std::string& payload);
void checkWiFiConnection();
uint16_t decodeLittleEndianU16(uint8_t lowByte, uint8_t highByte);
void testDisplay();

// WiFi reconnection function
void checkWiFiConnection() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi disconnected. Attempting to reconnect...");
        WiFi.disconnect();
        delay(1000);
        
        // Try to reconnect
        WiFi.begin();
        
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 20) {
            delay(500);
            Serial.print(".");
            attempts++;
        }
        
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println();
            Serial.print("WiFi reconnected! IP: ");
            Serial.println(WiFi.localIP());
            Serial.print("Signal strength (RSSI): ");
            Serial.println(WiFi.RSSI());
        } else {
            Serial.println();
            Serial.println("Failed to reconnect to WiFi");
        }
    }
}

void blinkRedGreen(int duration) {
    unsigned long start = millis();
    unsigned long prevBlinkMillis = 0;
    int interval = 999; // Blink interval in milliseconds
    bool isRedOnLed4 = true; // Start with LED 4 as red

    while (millis() - start < duration) {
        unsigned long currentMillis = millis();

        // Check if it's time to toggle the color
        if (currentMillis - prevBlinkMillis >= interval) {
            prevBlinkMillis = currentMillis; // Save the current time

            if (isRedOnLed4) {
                leds[4] = CRGB::Cyan;   // Set LED at index 4 to red
                leds[5] = CRGB::Magenta; // Set LED at index 5 to green
            } else {
                leds[4] = CRGB::Magenta; // Set LED at index 4 to green
                leds[5] = CRGB::Cyan;   // Set LED at index 5 to red
            }

            FastLED.show();
            isRedOnLed4 = !isRedOnLed4; // Toggle the LED colors
        }

        // Do other non-blocking tasks here if needed
    }
}

// Function to write values to EEPROM
void writeDryValueToEEPROM(int value) {
    EEPROM.writeInt(DRY_VALUE_ADDRESS, value);
    EEPROM.commit();
}

void writeWetValueToEEPROM(int value) {
    EEPROM.writeInt(WET_VALUE_ADDRESS, value);
    EEPROM.commit();
}

// Function to read values from EEPROM
int readDryValueFromEEPROM() {
    int value = EEPROM.readInt(DRY_VALUE_ADDRESS);
    return (value == 0 || value == -1) ? 3000 : value; // Default if not set
}

int readWetValueFromEEPROM() {
    int value = EEPROM.readInt(WET_VALUE_ADDRESS);
    return (value == 0 || value == -1) ? 1000 : value; // Default if not set
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

// Function to handle calibration form
void handleCalibration() {
    String dryVal = server.arg("dryValue");
    String wetVal = server.arg("wetValue");
    
    if (dryVal.length() > 0) {
        dryValue = dryVal.toInt();
        writeDryValueToEEPROM(dryValue);
    }
    
    if (wetVal.length() > 0) {
        wetValue = wetVal.toInt();
        writeWetValueToEEPROM(wetValue);
    }
    
    server.send(200, "text/html", "<html><body><h1>Calibration Values Saved!</h1><a href='/'>Back</a></body></html>");
}

// Function to make an HTTP request and parse JSON with retry and timeout settings
String getSongTitle() {
    // Check and reconnect WiFi if needed
    checkWiFiConnection();
    
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi still not connected, cannot fetch song title");
        return "WiFi disconnected";
    }
    
    Serial.println("Attempting to connect to: " + String(apiUrl));
    
    HTTPClient http;
    
    // Configure HTTP client with timeouts
    http.setTimeout(10000); // 10 second timeout
    http.setConnectTimeout(5000); // 5 second connection timeout
    http.setReuse(false); // Don't reuse connections
    
    // Add headers
    http.addHeader("User-Agent", "ESP32-Snowflake/1.0");
    http.addHeader("Accept", "application/json");
    
    if (!http.begin(apiUrl)) {
        Serial.println("HTTP begin failed");
        return "HTTP begin error";
    }
    
    int httpResponseCode = http.GET();
    Serial.print("HTTP Response Code: ");
    Serial.println(httpResponseCode);

    if (httpResponseCode > 0) {
        String payload = http.getString();
        Serial.println("HTTP Response received, length: " + String(payload.length()));
        
        if (payload.length() > 0) {
            Serial.println("HTTP Response: " + payload.substring(0, min(200, (int)payload.length()))); // Show first 200 chars

            // Parse JSON
            StaticJsonDocument<1024> doc;
            DeserializationError error = deserializeJson(doc, payload);

            if (error) {
                Serial.print("JSON Deserialization failed: ");
                Serial.println(error.f_str());
                http.end();
                return "Error parsing JSON";
            }

            // Extract the song title from the JSON response
            const char* songTitle = doc["TrackTitle"];
            http.end();
            return songTitle ? String(songTitle) : "No song data";
        } else {
            Serial.println("Empty response received");
            http.end();
            return "Empty response";
        }
    } else {
        Serial.print("HTTP GET request failed, error: ");
        Serial.println(http.errorToString(httpResponseCode).c_str());
        Serial.print("WiFi status: ");
        Serial.println(WiFi.status());
        http.end();
        return "HTTP error";
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
        displayCenteredText(String((int)round(temperature)), 2, 32);
    }
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

// FIXED: Function to display centered text on the OLED screen
void displayCenteredText(String text, int textSize, int yPosition) {
    display.clearDisplay();
    display.setTextSize(textSize);
    display.setTextColor(WHITE, BLACK);
    display.setTextWrap(false);
    
    // For 64x48 display - much smaller calculations needed
    int charWidth = 6 * textSize;  // Each character width including spacing
    int charHeight = 8 * textSize; // Each character height
    
    int textWidth = text.length() * charWidth;
    
    // Center horizontally on 64-pixel wide screen
    int xPosition = (SCREEN_WIDTH - textWidth) / 2;
    
    // For 48-pixel tall screen, adjust Y positioning
    int finalYPosition = yPosition;
    if (yPosition == -1) {
        // Center vertically
        finalYPosition = (SCREEN_HEIGHT - charHeight) / 2;
    }
    
    // Bounds checking for small screen
    if (xPosition < 0) xPosition = 0;
    if (xPosition + textWidth > SCREEN_WIDTH) xPosition = SCREEN_WIDTH - textWidth;
    if (xPosition < 0) xPosition = 0; // If text is too wide, align left
    
    if (finalYPosition < 0) finalYPosition = 0;
    if (finalYPosition + charHeight > SCREEN_HEIGHT) finalYPosition = SCREEN_HEIGHT - charHeight;
    
    display.setCursor(xPosition, finalYPosition);
    display.print(text);
    display.display();
    
    Serial.print("64x48 Display - Text: '");
    Serial.print(text);
    Serial.print("' Size: ");
    Serial.print(textSize);
    Serial.print(" Pos: (");
    Serial.print(xPosition);
    Serial.print(",");
    Serial.print(finalYPosition);
    Serial.print(") TextWidth: ");
    Serial.print(textWidth);
    Serial.print(" Screen: ");
    Serial.print(SCREEN_WIDTH);
    Serial.print("x");
    Serial.println(SCREEN_HEIGHT);
}

void updateWindSpeed() {
    // Check and reconnect WiFi if needed
    checkWiFiConnection();
    
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi still not connected, cannot fetch wind speed");
        return;
    }
    
    HTTPClient client;
    String url = "https://api.holfuy.com/live/?s=214&pw=correcthorsebatterystaple&m=JSON&tu=C&su=m/s";
    
    Serial.println("Attempting to connect to Holfuy API for wind data: " + url);
    
    // Configure HTTP client with timeouts
    client.setTimeout(10000); // 10 second timeout
    client.setConnectTimeout(5000); // 5 second connection timeout
    client.setReuse(false); // Don't reuse connections
    
    // Add headers
    client.addHeader("User-Agent", "ESP32-Snowflake/1.0");
    client.addHeader("Accept", "application/json");

    // Begin the HTTP request to the API
    if (!client.begin(url)) {
        Serial.println(F("Holfuy API connection failed"));
        return;
    }

    // Send the HTTP GET request
    int httpCode = client.GET();
    Serial.print("Holfuy HTTP Response Code: ");
    Serial.println(httpCode);

    // Check if the GET request was successful
    if (httpCode > 0) {
        String payload = client.getString();  // Get the response payload
        
        // Print the response to the Serial monitor
        Serial.println("Holfuy Response length: " + String(payload.length()));
        if (payload.length() > 0) {
            Serial.println("Holfuy Response: " + payload.substring(0, min(200, (int)payload.length())));

            // Parse the JSON response
            StaticJsonDocument<512> doc;
            DeserializationError error = deserializeJson(doc, payload);

            if (!error) {
                // Successfully parsed the JSON, get the wind speed
                float speed = doc["wind"]["speed"];
                Serial.print("Wind Speed Holfuy: ");
                Serial.print(speed);
                Serial.println(" m/s");

                // Set the LED color based on the wind speed
                setWindSpeedLEDColor(speed);
                windSpeed = speed;  // Store the wind speed
            } else {
                // Handle JSON parsing error
                Serial.print(F("JSON deserialization failed: "));
                Serial.println(error.f_str());
            }
        } else {
            Serial.println("Empty response from Holfuy API");
        }
    } else {
        // If the GET request failed
        Serial.println("Holfuy HTTP request failed, error code: " + String(httpCode));
        Serial.print("WiFi status: ");
        Serial.println(WiFi.status());
    }

    // Close the connection
    client.end();
}

void setWindSpeedLEDColor(float windSpeed) {
    // LEDs at index 2 and 3 for wind speed (0-25+ m/s scale)
    if (windSpeed >= 0 && windSpeed < 3) {
        fill_solid(leds + 2, 2, 0xFF44DD);  // Pink (0-3 m/s)
    } else if (windSpeed >= 3 && windSpeed < 6) {
        fill_solid(leds + 2, 2, 0x0006FF);  // Blue (3-6 m/s)
    } else if (windSpeed >= 6 && windSpeed <= 9) {
        fill_solid(leds + 2, 2, 0x00FF06);  // Green (6-9 m/s)
    } else if (windSpeed > 9 && windSpeed <= 12) {
        fill_solid(leds + 2, 2, 0xFFF600);  // Yellow (9-12 m/s)
    } else if (windSpeed > 12 && windSpeed <= 18) {
        fill_solid(leds + 2, 2, 0xFFA500);  // Orange (12-18 m/s)
    } else if (windSpeed > 18 && windSpeed <= 22) {
        fill_solid(leds + 2, 2, 0xFF0000);  // Red (18-22 m/s)
    } else if (windSpeed > 22) {
        fill_solid(leds + 2, 2, 0x800080);  // Purple (22+ m/s)
    }

    FastLED.show();
}

void setTemperatureLEDColor(float roundedTemperature) {
    // First two LEDs for temperature
    if (roundedTemperature >= -50 && roundedTemperature < -5) {
        fill_solid(leds, 2, 0xFF44DD);  // Pink
    } else if (roundedTemperature >= -5 && roundedTemperature < 0) {
        fill_solid(leds, 2, 0x0006FF);  // Blue
    } else if (roundedTemperature >= 0 && roundedTemperature <= 5) {
        fill_solid(leds, 2, 0x00FF06);  // Green
    } else if (roundedTemperature > 5 && roundedTemperature <= 10) {
        fill_solid(leds, 2, 0xFFF600);  // Yellow
    } else if (roundedTemperature > 10 && roundedTemperature <= 15) {
        fill_solid(leds, 2, 0xFFA500);  // Orange
    } else if (roundedTemperature > 15 && roundedTemperature <= 20) {
        fill_solid(leds, 2, 0xFF0000);  // Red
    } else if (roundedTemperature > 20 && roundedTemperature <= 45) {
        fill_solid(leds, 2, 0x800080);  // Purple
    } else {
        fill_solid(leds, 2, 0x000000);  // Off
    }

    FastLED.show();
}

void handleMoistureLEDs() {
    unsigned long currentMillis = millis();
    
    // Check if 45 seconds have passed since the last moisture check
    if (currentMillis - previousMoistureCheckMillis >= sensorCheckInterval) {
        previousMoistureCheckMillis = currentMillis;  // Update the last check time

        Serial.println("Reading moisture sensor...");
        moistureValue = analogRead(MOISTURE_PIN);
        Serial.print("Moisture Value: ");
        Serial.println(moistureValue);
        Serial.print("Dry threshold: ");
        Serial.println(dryValue);
        Serial.print("Wet threshold: ");
        Serial.println(wetValue);

        // Calculate moisture percentage (inverted because lower values = more moisture)
        float moisturePercent = 0;
        if (dryValue != wetValue) {
            moisturePercent = ((float)(dryValue - moistureValue) / (float)(dryValue - wetValue)) * 100;
            moisturePercent = constrain(moisturePercent, 0, 100);
        }
        
        Serial.print("Moisture percentage: ");
        Serial.println(moisturePercent);

        // Handle moisture LED updates for the last three LEDs (indices 3, 4, 5)
        if (moisturePercent <= 30) {
            fill_solid(leds + 3, 3, CRGB::Red);    // Dry: Red
        } else if (moisturePercent > 30 && moisturePercent <= 70) {
            fill_solid(leds + 3, 3, CRGB::Yellow); // OK: Yellow
        } else {
            fill_solid(leds + 3, 3, CRGB::Green);  // Wet: Green
        }

        FastLED.show();
    }
}

void handleRoot() {
    // Get the current song title from the API
    String currentSongTitle = getSongTitle();
    String storedSongTitle = readSongTitleFromEEPROM(); // Get the stored song title from EEPROM

    // Format the values for display
    String temperatureString = String(temperature, 1) + " °C"; // Format temperature
    String windSpeedString = String(windSpeed, 1) + " m/s"; // Format wind speed
    
    // Calculate moisture percentage for display
    float moisturePercent = 0;
    if (dryValue != wetValue) {
        moisturePercent = ((float)(dryValue - moistureValue) / (float)(dryValue - wetValue)) * 100;
        moisturePercent = constrain(moisturePercent, 0, 100);
    }

    // Generate the HTML content
    String htmlContent = R"rawliteral(
        <html>
        <head>
            <meta charset="UTF-8">
            <meta name="viewport" content="width=device-width, initial-scale=1.0">
            <title>Snowflake</title>
            <style>
                body {
                    font-family: Arial, sans-serif;
                    margin: 0;
                    padding: 20px;
                    background-color: #f4f4f9;
                    color: #333;
                    display: flex;
                    flex-direction: column;
                    align-items: center;
                }
                h1 {
                    color: #007bff;
                    text-align: center;
                }
                b {
                    
                    text-align: center;
                }
                p {
                    margin: 10px 0;
                }
                .container {
                    max-width: 600px;
                    width: 100%;
                    background-color: #fff;
                    padding: 20px;
                    border-radius: 10px;
                    box-shadow: 0 2px 8px rgba(0, 0, 0, 0.1);
                    box-sizing: border-box;
                }
                label {
                    display: block;
                    margin: 10px 0 5px;
                    font-weight: bold;
                }
                input[type="text"], input[type="number"] {
                    width: 100%;
                    padding: 10px;
                    margin-bottom: 15px;
                    border: 1px solid #ccc;
                    border-radius: 5px;
                    box-sizing: border-box;
                }
                input[type="submit"] {
                    background-color: #007bff;
                    color: #fff;
                    border: none;
                    padding: 10px 20px;
                    border-radius: 5px;
                    cursor: pointer;
                    font-size: 16px;
                    width: 100%;
                    margin-bottom: 10px;
                }
                input[type="submit"]:hover {
                    background-color: #0056b3;
                }
                a {
                    display: block;
                    text-align: center;
                    margin-top: 20px;
                    color: #007bff;
                    text-decoration: none;
                    font-weight: bold;
                }
                a:hover {
                    color: #0056b3;
                }
            </style>
        </head>
        <body>
            <div class="container">
                <h1>Snowflake Lamp</h1>
                <p><b>BLE temperature: )rawliteral" + temperatureString + R"rawliteral(</b></p>
                <p><b>Wind Speed: )rawliteral" + windSpeedString + R"rawliteral(</b></p>
                <p><b>Soil Moisture: )rawliteral" + String(moisturePercent, 1) + R"rawliteral(% (Raw: )rawliteral" + String(moistureValue) + R"rawliteral()</b></p>
                
                <h2>Soil Moisture Calibration</h2>
                <form action="/calibration" method="post">
                    <label for="dryValue">Dry Value (sensor in dry soil):</label>
                    <input type="number" id="dryValue" name="dryValue" value=")rawliteral" + String(dryValue) + R"rawliteral(">
                    <label for="wetValue">Wet Value (sensor in wet soil):</label>
                    <input type="number" id="wetValue" name="wetValue" value=")rawliteral" + String(wetValue) + R"rawliteral(">
                    <input type="submit" value="Save Calibration">
                </form>
                
                <h2>Whamageddon warning - Mix Megapol</h2>
                <p>Playing now: )rawliteral" + currentSongTitle + R"rawliteral(</p>
                <p>Warning for: )rawliteral" + storedSongTitle + R"rawliteral(</p>
                <form action="/setSongTitle" method="post">
                    <label for="songTitle">Set Song Title:</label>
                    <input type="text" id="songTitle" name="songTitle" value=")rawliteral" + storedSongTitle + R"rawliteral(">
                    <input type="submit" value="Save">
                </form>
                <a href="/update">Update Firmware</a>
            </div>
        </body>
        </html>
    )rawliteral";

    // Send the web page
    server.send(200, "text/html", htmlContent);
}

// FIXED: setup function with improved OLED initialization
void setup() {
    Serial.begin(115200);
    Serial.println("Starting Snowflake device...");
    
    // Initialize WiFi
    WiFiManager wifiManager;
    Serial.println("Connecting to WiFi...");
    wifiManager.autoConnect("Snowflake");
    
    Serial.print("WiFi connected! IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Signal strength (RSSI): ");
    Serial.println(WiFi.RSSI());
    
    // Initialize I2C with explicit pins
    Wire.begin(OLED_SDA, OLED_SCL);
    Serial.println("I2C initialized");
    
    // Scan for I2C devices (debugging)
    Serial.println("Scanning for I2C devices...");
    byte error, address;
    int nDevices = 0;
    for(address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        error = Wire.endTransmission();
        if (error == 0) {
            Serial.print("I2C device found at address 0x");
            if (address < 16) Serial.print("0");
            Serial.println(address, HEX);
            nDevices++;
        }
    }
    if (nDevices == 0) {
        Serial.println("No I2C devices found");
    }
    
    // Initialize display with error checking for older library
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    Serial.println("SSD1306 initialized");
    
    // Clear display and set initial state
    display.clearDisplay();
    display.setRotation(0);
    display.display(); // Show cleared display
    
    delay(1000); // Give display time to initialize
    
    // Test display with simple text
    Serial.println("Testing display...");
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.print("Kasewood");
    display.display();
    delay(2000);
    
    FastLED.addLeds<WS2811, DATA_PIN, RGB>(leds, NUM_LEDS);
    FastLED.setBrightness(200);
    FastLED.setMaxPowerInVoltsAndMilliamps(5, 500);  // Limit power to 5V and 500 mA

    EEPROM.begin(512);

    // Load calibration values from EEPROM
    dryValue = readDryValueFromEEPROM();
    wetValue = readWetValueFromEEPROM();
    Serial.print("Loaded dry value: ");
    Serial.println(dryValue);
    Serial.print("Loaded wet value: ");
    Serial.println(wetValue);

    server.on("/", HTTP_GET, handleRoot);  // Handle GET requests to '/'
    server.on("/setSongTitle", HTTP_POST, handleSetSongTitle);  // Handle POST requests to '/setSongTitle'
    server.on("/calibration", HTTP_POST, handleCalibration);  // Handle calibration form

    ElegantOTA.begin(&server);
    server.begin();
    Serial.println("Web server started");

    Serial.println("Moisture sensor initialized on GPIO 32");

    NimBLEDevice::init("");
    pBLEScan = NimBLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks(), true);
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);
    pBLEScan->setActiveScan(true);
    Serial.println("BLE scan initialized");

    // Display "HEJ" on boot with better error handling
    Serial.println("Displaying HEJ...");
    displayCenteredText("HEJ", 2, 32);
    Serial.println("Setup completed");
}

void loop() {
    unsigned long currentMillis = millis();

    // Check WiFi connection periodically
    static unsigned long lastWiFiCheck = 0;
    if (currentMillis - lastWiFiCheck >= 30000) { // Check every 30 seconds
        lastWiFiCheck = currentMillis;
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("WiFi disconnected in main loop");
            checkWiFiConnection();
        }
    }

    // Set unified sensor check interval to 45 seconds (45,000 milliseconds)
    if (currentMillis - previousTitleCheckMillis >= sensorCheckInterval) {
        previousTitleCheckMillis = currentMillis;

        // Check for the current song title
        String storedSongTitle = readSongTitleFromEEPROM();  // Get the stored song title
        String currentSongTitle = getSongTitle();  // Get the current track title from API

        // If the stored song title matches the current track title, start blinking red LEDs
        if (caseInsensitiveCompare(storedSongTitle.c_str(), currentSongTitle.c_str())) {
            isBlinking = true; // Set flag to indicate LED control for blinking
        } else {
            isBlinking = false; // Clear flag to return to normal LED behavior
        }
    }

    // Handle LED behavior based on the flag
    if (isBlinking) {
        // Blink the LEDs red continuously if the flag is set
        blinkRedLEDs();
    } else {
        // Normal LED handling and temperature display when not blinking
        if (!hasReceivedReading) {
            fill_solid(leds, NUM_LEDS, CRGB::White);
            FastLED.show();
            displayCenteredText("OFF", 2, 32);
        } else {
            handleMoistureLEDs();
        }
    }

    // Existing BLE scan logic
    if (currentMillis - previousBLEMillis >= scanInterval) {
        previousBLEMillis = currentMillis;
        pBLEScan->start(scanTime, false);
        Serial.println("Scanning...");
    }

    // Update wind speed every 45 seconds
    if (currentMillis - previousWindUpdateMillis >= sensorCheckInterval) {
        previousWindUpdateMillis = currentMillis;
        updateWindSpeed(); // Update wind speed every 45 seconds
    }

    // Handle OTA updates and web server requests
    ElegantOTA.loop();
    server.handleClient();
}

// Non-blocking blinkRedLEDs function
void blinkRedLEDs() {
    static unsigned long previousBlinkMillis = 0;
    static bool isRed = false;
    unsigned long currentMillis = millis();

    if (currentMillis - previousBlinkMillis >= 500) { // 500ms interval
        previousBlinkMillis = currentMillis;
        isRed = !isRed; // Toggle between red and off

        if (isRed) {
            fill_solid(leds, NUM_LEDS, CRGB::Red); // Turn all LEDs red
        } else {
            fill_solid(leds, NUM_LEDS, CRGB::Black); // Turn off LEDs
        }
        FastLED.show();
    }
}

// Debug function to test display functionality
void testDisplay() {
    Serial.println("Running display test sequence...");
    
    // Test 1: Clear display
    display.clearDisplay();
    display.display();
    delay(500);
    
    // Test 2: Single pixel
    display.clearDisplay();
    display.drawPixel(64, 32, WHITE);
    display.display();
    delay(1000);
    
    // Test 3: Simple text
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.print("Kasewood");
    display.display();
    delay(2000);
    
    // Test 4: Centered large text
    displayCenteredText("OK", 3, 20);
    delay(2000);
    
    Serial.println("Display test sequence completed");
}
