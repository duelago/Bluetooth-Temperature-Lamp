
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

// Set the interval for all sensor checks to 45 seconds (45,000 milliseconds)

unsigned long previousCO2CheckMillis = 0;
unsigned long previousTitleCheckMillis = 0;
unsigned long previousTemperatureUpdateMillis = 0; // Add a variable to track temperature update time
const unsigned long sensorCheckInterval = 45000; // 45 seconds (45,000 milliseconds)

const char* apiUrl = "https://listenapi.planetradio.co.uk/api9.2/nowplaying/mme";

unsigned long previousMillis = 0; // Stores the last time BLE scan was triggered
const unsigned long scanInterval = 10000; // Time between BLE scans (e.g., 10 seconds)


const char* apiHost = "api.holfuy.com";

// The updated interval for all checks
const unsigned long updateInterval = 45000;  // 45 seconds in milliseconds


float temperature = 0.0;
bool hasReceivedReading = false;
const std::string targetMacAddress = "38:1f:8d:97:bd:5d";

//bool blockLEDUpdates = false; // Flag to block other LED updates
// Global flag to control LED behavior
bool isBlinking = false;


NimBLEScan* pBLEScan;
const int scanTime = 5;

void blinkRedGreen(int duration) {
    unsigned long start = millis();
    unsigned long previousMillis = 0;
    int interval = 999; // Blink interval in milliseconds
    bool isRedOnLed4 = true; // Start with LED 4 as red

    while (millis() - start < duration) {
        unsigned long currentMillis = millis();

        // Check if it's time to toggle the color
        if (currentMillis - previousMillis >= interval) {
            previousMillis = currentMillis; // Save the current time

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


// Function to make an HTTP request and parse JSON with retry and timeout settings

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
void blinkRedGreen(int duration);
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

void updateTemperature() {
    HTTPClient client;

    // Begin the HTTP request to the API
    if (!client.begin("https://api.holfuy.com/live/?s=214&pw=correcthorsebatterystaple&m=JSON&tu=C&su=m/s")) {
        Serial.println(F("Connection failed"));
        return;
    }

    // Send the HTTP GET request
    int httpCode = client.GET();

    // Check if the GET request was successful
    if (httpCode > 0) {
        String payload = client.getString();  // Get the response payload

        // Print the response to the Serial monitor
        Serial.println("Response: " + payload);

        // Parse the JSON response
        StaticJsonDocument<384> doc;
        DeserializationError error = deserializeJson(doc, payload);

        if (!error) {
            // Successfully parsed the JSON, get the temperature
            float temp = doc["temperature"];
            Serial.print("Temp Holfuy: ");
            Serial.println(temp);

            // Round the temperature and print
            int roundedTemperature = round(temp);
            Serial.print("Rounded Temp: ");
            Serial.println(roundedTemperature);

            // Set the LED color based on the temperature
            setTemperatureLEDColorHolfuy(roundedTemperature);
            temperature = roundedTemperature;  // Store the temperature
        } else {
            // Handle JSON parsing error
            Serial.print(F("JSON deserialization failed: "));
            Serial.println(error.f_str());
        }
    } else {
        // If the GET request failed
        Serial.println("HTTP request failed, error code: " + String(httpCode));
    }

    // Close the connection
    client.end();
}


void setTemperatureLEDColorHolfuy(float roundedTemperature) {
    if (roundedTemperature >= -50 && roundedTemperature < -5) {
        fill_solid(leds + 2, 2, 0xFF44DD);  // Pink, start from index 2 and cover 2 LEDs
    } else if (roundedTemperature >= -5 && roundedTemperature < 0) {
        fill_solid(leds + 2, 2, 0x0006FF);  // Blue
    } else if (roundedTemperature >= 0 && roundedTemperature <= 5) {
        fill_solid(leds + 2, 2, 0x00FF06);  // Green
    } else if (roundedTemperature > 5 && roundedTemperature <= 10) {
        fill_solid(leds + 2, 2, 0xFFF600);  // Yellow
    } else if (roundedTemperature > 10 && roundedTemperature <= 15) {
        fill_solid(leds + 2, 2, 0xFFA500);  // Orange
    } else if (roundedTemperature > 15 && roundedTemperature <= 20) {
        fill_solid(leds + 2, 2, 0xFF0000);  // Red
    } else if (roundedTemperature > 20 && roundedTemperature <= 45) {
        fill_solid(leds + 2, 2, 0x800080);  // Purple
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

void handleCO2LEDs() {
    unsigned long currentMillis = millis();
    
    // Check if 30 seconds have passed since the last CO2 check
    if (currentMillis - previousCO2CheckMillis >= sensorCheckInterval) {
        previousCO2CheckMillis = currentMillis;  // Update the last check time

        if (scd4x.readMeasurement()) {
            uint16_t currentCO2 = scd4x.getCO2();
            Serial.print("CO2 Level: ");
            Serial.println(currentCO2);

            // Handle CO2 LED updates for the last two LEDs
            if (currentCO2 <= 650) {
                fill_solid(leds + 4, 2, CRGB::Green);  // CO2: <= 650 ppm, Green
            } else if (currentCO2 > 650 && currentCO2 <= 800) {
                fill_solid(leds + 4, 2, CRGB::Yellow); // CO2: 651 - 800 ppm, Yellow
            } else if (currentCO2 > 800 && currentCO2 <= 1000) {
                fill_solid(leds + 4, 2, CRGB::Orange); // CO2: 801 - 1000 ppm, Orange
            } else if (currentCO2 > 1000 && currentCO2 <= 1200) {
                fill_solid(leds + 4, 2, CRGB::Red);    // CO2: 1001 - 1200 ppm, Red
            } else if (currentCO2 > 1200 && currentCO2 <= 1299) {
                fill_solid(leds + 4, 2, CRGB::Purple); // CO2: 1201 - 1299 ppm, Purple
            } else {
                blinkRedGreen(20000);
            }

            FastLED.show();
        }
    }
}



void handleRoot() {
    // Get the current song title from the API
    String currentSongTitle = getSongTitle();
    String storedSongTitle = readSongTitleFromEEPROM(); // Get the stored song title from EEPROM

   



    // Format the values for display
    String temperatureString = String(temperature, 1) + " °C"; // Format temperature
    String co2String = String(currentCO2) + " ppm"; // Format CO2 level

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
                input[type="text"] {
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
                <p><b>Local temperature: )rawliteral" + temperatureString + R"rawliteral(</b></p>
                <p><b>CO2 Level in ppm: )rawliteral" + scd4x.getCO2() + R"rawliteral(</b></p>
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
    unsigned long currentMillis = millis();

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
        // Normal LED handling and CO2 display when not blinking
        if (!hasReceivedReading) {
            fill_solid(leds, NUM_LEDS, CRGB::White);
            FastLED.show();
            displayCenteredText("OFF", 2, 15);
        } else {
            handleCO2LEDs();
        }
    }

    // Existing BLE scan logic
    if (currentMillis - previousMillis >= scanInterval) {
        previousMillis = currentMillis;
        pBLEScan->start(scanTime, false);
        Serial.println("Scanning...");
    }

    // Update temperature every 45 seconds
    if (currentMillis - previousTemperatureUpdateMillis >= sensorCheckInterval) {
        previousTemperatureUpdateMillis = currentMillis;
        updateTemperature(); // Update temperature every 45 seconds
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
