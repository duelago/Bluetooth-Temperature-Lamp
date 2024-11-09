#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <FastLED.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFiManager.h>
#include <ESPmDNS.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ElegantOTA.h>

char ssid[] = "";       // Leave empty to configure with WiFi Manager
char password[] = "";   // Leave empty to configure with WiFi Manager

#define OLED_RESET -1   // Use -1 for ESP32, no reset pin
#define NUM_LEDS 1
#define DATA_PIN 2

CRGB leds[NUM_LEDS];

#define OLED_RESET -1 // No reset pin for the ESP32
Adafruit_SSD1306 display(128, 64, &Wire, OLED_RESET);


WiFiClientSecure client;
#define TEST_HOST "api.holfuy.com"

WebServer server(80);

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

const char html[] PROGMEM = R"(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>Temperaturlampan</title>
    <style>
        html, body { height: 100%; background-color: #90c0de; overflow: hidden; }
        time {
            position: absolute;
            top: 50%;
            left: 0;
            right: 0;
            margin: -110px 0 0 0;
            height: 220px;
            text-align: center;
            color: #1c7bb7;
            font-family: Arial;
            font-size: 260px;
            line-height: 227px;
            font-weight: bold;
        }
    </style>
</head>
<body>
    <time><span id="temperature">--</span></time>
    <script>
        function updateTemperature(temperature) {
            document.getElementById('temperature').textContent = temperature;
        }
    </script>
    <p><a href='/update'>Firmwareuppdatering</a></p>
</body>
</html>
)";

float temperature = 0.0;

void setup() {
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    display.display();
    delay(2000);
    display.clearDisplay();

    Serial.begin(115200);
    if (!MDNS.begin("lampan")) {
        Serial.println("Error starting mDNS");
    }
    ElegantOTA.begin(&server);
    FastLED.addLeds<WS2811, DATA_PIN, RGB>(leds, NUM_LEDS);
    FastLED.setBrightness(100);

    WiFiManager wifiManager;
    wifiManager.autoConnect("Temperaturlampan");

    IPAddress ip = WiFi.localIP();
    String lastOctet = String(ip[3]);
    displayCenteredText(lastOctet, 2, 15);
    delay(15000);

    display.clearDisplay();
    display.display();

    server.on("/", HTTP_GET, []() {
        float temperature = makeHTTPRequest();
        String htmlWithTemp = String(html);
        htmlWithTemp.replace("--", String(temperature));
        server.send(200, "text/html", htmlWithTemp);
    });

    server.begin();
    makeHTTPRequest();
}

void loop() {
    
    ElegantOTA.loop();
    server.handleClient();

    static unsigned long lastUpdateTime = 0;
    const unsigned long updateInterval = 300000; // 1 minute in milliseconds
    unsigned long currentTime = millis();

    if (currentTime - lastUpdateTime >= updateInterval) {
        lastUpdateTime = currentTime;
        makeHTTPRequest();
    }
}

float makeHTTPRequest() {
    client.setInsecure();
    if (!client.connect(TEST_HOST, 443)) {
        Serial.println(F("Connection failed"));
        return 0.0;
    }

    yield();

    client.print(F("GET "));
    client.print("/live/?s=214&pw=correcthorsebatterystaple&m=JSON&tu=C&su=m/s");
    client.println(F(" HTTP/1.1"));
    client.print(F("Host: "));
    client.println(TEST_HOST);
    client.println(F("Cache-Control: no-cache"));

    if (client.println() == 0) {
        Serial.println(F("Failed to send request"));
        return 0.0;
    }

    char status[32] = {0};
    client.readBytesUntil('\r', status, sizeof(status));
    if (strcmp(status, "HTTP/1.1 200 OK") != 0) {
        Serial.print(F("Unexpected response: "));
        Serial.println(status);
        return 0.0;
    }

    char endOfHeaders[] = "\r\n\r\n";
    if (!client.find(endOfHeaders)) {
        Serial.println(F("Invalid response"));
        return 0.0;
    }

    StaticJsonDocument<384> doc;
    DeserializationError error = deserializeJson(doc, client);

    if (!error) {
        float temp = doc["temperature"];
        Serial.print("Temp: ");
        Serial.println(temp);

        int roundedTemperature = round(temp);
        Serial.print("Temp rounded: ");
        Serial.println(roundedTemperature);

        setLEDColor(roundedTemperature);
        temperature = roundedTemperature;

        display.cp437(true);
        display.clearDisplay();
        display.setTextSize(2);
        display.setTextColor(WHITE);
        displayCenteredText(String(roundedTemperature), 2, 15);
        display.display();

        return temperature;
    } else {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return 0.0;
    }
    leds[0] = CRGB::Black;
    delay(100);
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
    FastLED.delay(100);
}
