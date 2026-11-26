// Snowflake Lamp
// LED 0-1: temperatur från BLE-sensor (inomhus)
// LED 2-3: temperatur från Holfuy
// LED 4-5: CO2 från SCD40
// Alla LED: blinkar rött medan Whamageddon-låten spelas på Mix Megapol
//
// Bibliotek (Arduino IDE): ESP32-core 3.x, NimBLE-Arduino 2.x,
// ArduinoJson 7.x, WiFiManager 2.x, ElegantOTA 3.x, FastLED 3.x,
// samt det egna hårdkodade Adafruit_SSD1306 i libraries-mappen

#include <FastLED.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SparkFun_SCD4x_Arduino_Library.h>
#include <NimBLEDevice.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <ElegantOTA.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <EEPROM.h>

// ---------------------------------------------------------------------------
// Hårdvara
// ---------------------------------------------------------------------------
#define OLED_RESET -1
#define NUM_LEDS   6
#define DATA_PIN   4

#define OLED_SDA 21
#define OLED_SCL 22
#define CO2_SDA  17
#define CO2_SCL  16

#define TEXT_Y 32   // OLED-rad där siffrorna ritas

// ---------------------------------------------------------------------------
// Inställningar
// ---------------------------------------------------------------------------
#define SONG_TITLE_ADDRESS 0
#define SONG_TITLE_MAX_LEN 32            // inkl. avslutande '\0'

const char* hostName  = "snowflake";     // http://snowflake.local
const char* songApiUrl = "https://listenapi.planetradio.co.uk/api9.2/nowplaying/mme";
const char* holfuyUrl  = "https://api.holfuy.com/live/?s=214&pw=correcthorsebatterystaple&m=JSON&tu=C&su=m/s";

const std::string targetMacAddress = "34:ec:b6:65:16:fb";   // BTH01 (pvvx BTHome)

const unsigned long sensorCheckInterval       = 45000;   // CO2, Holfuy, låt (normalt)
const unsigned long songCheckIntervalBlinking = 15000;   // låt, medan den spelas
const unsigned long bleScanInterval           = 10000;   // tid mellan BLE-scanningar
const unsigned long bleScanDurationMs         = 5000;
const unsigned long bleStaleTimeout           = 10UL * 60UL * 1000UL; // 10 min utan BLE-data -> OFF
const unsigned long wifiRetryInterval         = 60000;
const unsigned long httpTimeoutMs             = 4000;
const unsigned long co2AlarmBlinkInterval     = 1000;
const unsigned long whamBlinkInterval         = 500;

const uint16_t co2AlarmLevel = 1300;     // över detta blinkar LED 4-5 cyan/magenta

// ---------------------------------------------------------------------------
// Globala objekt
// ---------------------------------------------------------------------------
WebServer server(80);
CRGB leds[NUM_LEDS];
// Äldre/hårdkodat Adafruit_SSD1306 (samma som i Elprispilen): upplösningen
// bestäms i bibliotekets header, inte här.
Adafruit_SSD1306 display(OLED_RESET);
SCD4x scd4x(SCD4x_SENSOR_SCD40);
NimBLEScan* pBLEScan;

// ---------------------------------------------------------------------------
// Tillstånd
// ---------------------------------------------------------------------------
// Satta av BLE-tasken, lästa i loop()
volatile bool  bleTempPending = false;
volatile float bleTemperature = 0.0;

bool          hasBleReading   = false;
unsigned long lastBleReadingMillis = 0;

float holfuyTemperature = 0.0;
bool  hasHolfuyReading  = false;

uint16_t currentCO2 = 0;
bool     hasCO2Reading = false;
bool     co2Alarm = false;

String lastSongTitle = "–";   // cache för webbsidan
bool   isBlinking = false;    // Whamageddon pågår

enum DisplayMode { MODE_UNKNOWN, MODE_OFF, MODE_NORMAL };
DisplayMode displayMode = MODE_UNKNOWN;

unsigned long previousCO2CheckMillis    = 0;
unsigned long previousTitleCheckMillis  = 0;
unsigned long previousHolfuyMillis      = 0;
unsigned long previousBleScanMillis     = 0;
unsigned long previousWifiRetryMillis   = 0;

// ---------------------------------------------------------------------------
// Färgtabeller
// ---------------------------------------------------------------------------
CRGB tempColor(int t) {
    if (t < -50 || t > 45) return CRGB::Black;  // utanför rimligt intervall
    if (t < -5)  return CRGB(0xFF44DD);   // rosa
    if (t < 0)   return CRGB(0x0006FF);   // blå
    if (t <= 5)  return CRGB(0x00FF06);   // grön
    if (t <= 10) return CRGB(0xFFF600);   // gul
    if (t <= 15) return CRGB(0xFFA500);   // orange
    if (t <= 20) return CRGB(0xFF0000);   // röd
    return CRGB(0x800080);                // lila (21-45)
}

struct CO2Band { uint16_t maxPpm; CRGB color; };
const CO2Band co2Bands[] = {
    {  650, CRGB::Green  },
    {  800, CRGB::Yellow },
    { 1000, CRGB::Orange },
    { 1200, CRGB::Red    },
    { 1299, CRGB::Purple },
};

// ---------------------------------------------------------------------------
// Hjälpfunktioner
// ---------------------------------------------------------------------------
void displayCenteredText(const String& text, int textSize, int yPosition) {
    display.clearDisplay();
    display.setTextSize(textSize);
    display.setTextColor(WHITE);

    int16_t x1, y1;
    uint16_t width, height;
    display.getTextBounds(text, 0, 0, &x1, &y1, &width, &height);

    // Centrera både horisontellt och vertikalt (yPosition används inte längre)
    display.setCursor((display.width() - width) / 2 - x1,
                      (display.height() - height) / 2 - y1);
    display.print(text);
    display.display();
}

String htmlEscape(const String& in) {
    String out;
    out.reserve(in.length() + 8);
    for (size_t i = 0; i < in.length(); i++) {
        char c = in[i];
        switch (c) {
            case '&':  out += "&amp;";  break;
            case '<':  out += "&lt;";   break;
            case '>':  out += "&gt;";   break;
            case '"':  out += "&quot;"; break;
            case '\'': out += "&#39;";  break;
            default:   out += c;
        }
    }
    return out;
}

// Gemener + trimmat, för jämförelse av låttitlar
String normalizeTitle(String s) {
    s.trim();
    s.toLowerCase();
    return s;
}

// HTTPS GET + JSON-parsning. Returnerar false vid nät-, HTTP- eller JSON-fel.
// Med filter sparas bara de fält som behövs (radio-API:ts svar är stort).
bool httpGetJson(const char* url, JsonDocument& doc, JsonDocument* filter = nullptr) {
    if (WiFi.status() != WL_CONNECTED) return false;

    WiFiClientSecure client;
    client.setInsecure();               // ingen certifikatkontroll (som tidigare, men nu uttryckligen)

    HTTPClient http;
    http.setTimeout(httpTimeoutMs);
    http.setConnectTimeout(httpTimeoutMs);

    if (!http.begin(client, url)) {
        Serial.println(F("HTTP begin failed"));
        return false;
    }

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Serial.printf("HTTP GET %s failed: %d (%s)\n", url, code, http.errorToString(code).c_str());
        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();

    DeserializationError error = filter
        ? deserializeJson(doc, payload, DeserializationOption::Filter(*filter))
        : deserializeJson(doc, payload);
    if (error) {
        Serial.print(F("JSON deserialization failed: "));
        Serial.println(error.c_str());
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// EEPROM – låttitel (max 31 tecken)
// ---------------------------------------------------------------------------
void writeSongTitleToEEPROM(const String& songTitle) {
    int len = min((int)songTitle.length(), SONG_TITLE_MAX_LEN - 1);
    for (int i = 0; i < len; i++) {
        EEPROM.write(SONG_TITLE_ADDRESS + i, songTitle[i]);
    }
    EEPROM.write(SONG_TITLE_ADDRESS + len, '\0');
    EEPROM.commit();
}

String readSongTitleFromEEPROM() {
    String songTitle = "";
    for (int i = 0; i < SONG_TITLE_MAX_LEN - 1; i++) {
        char c = EEPROM.read(SONG_TITLE_ADDRESS + i);
        if (c == '\0' || c == (char)0xFF) break;   // 0xFF = tomt EEPROM
        songTitle += c;
    }
    return songTitle;
}

// ---------------------------------------------------------------------------
// LED-logik
// ---------------------------------------------------------------------------
void setTempLEDs(int startIndex, float temperature) {
    fill_solid(leds + startIndex, 2, tempColor((int)round(temperature)));
}

void setCO2LEDs(uint16_t co2) {
    for (const CO2Band& band : co2Bands) {
        if (co2 <= band.maxPpm) {
            co2Alarm = false;
            fill_solid(leds + 4, 2, band.color);
            return;
        }
    }
    co2Alarm = true;   // blinkningen sköts av updateCO2Alarm()
}

// Icke-blockerande cyan/magenta-blink på LED 4-5 vid hög CO2
void updateCO2Alarm() {
    static unsigned long lastToggle = 0;
    static bool phase = false;
    if (!co2Alarm) return;
    if (millis() - lastToggle < co2AlarmBlinkInterval) return;
    lastToggle = millis();
    phase = !phase;
    leds[4] = phase ? CRGB::Cyan : CRGB::Magenta;
    leds[5] = phase ? CRGB::Magenta : CRGB::Cyan;
    FastLED.show();
}

// Icke-blockerande röd blink på alla LED:ar (Whamageddon)
void blinkRedLEDs() {
    static unsigned long previousBlinkMillis = 0;
    static bool isRed = false;
    if (millis() - previousBlinkMillis < whamBlinkInterval) return;
    previousBlinkMillis = millis();
    isRed = !isRed;
    fill_solid(leds, NUM_LEDS, isRed ? CRGB::Red : CRGB::Black);
    FastLED.show();
}

// Ritar alla LED:ar och displayen från senast kända värden
void renderNormal() {
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    if (hasBleReading)    setTempLEDs(0, bleTemperature);
    if (hasHolfuyReading) setTempLEDs(2, holfuyTemperature);
    if (hasCO2Reading)    setCO2LEDs(currentCO2);
    FastLED.show();
    if (hasBleReading) displayCenteredText(String((int)round(bleTemperature)), 2, TEXT_Y);
}

void renderOff() {
    co2Alarm = false;
    fill_solid(leds, NUM_LEDS, CRGB::White);
    FastLED.show();
    displayCenteredText("OFF", 2, TEXT_Y);
}

// Byter mellan OFF och normalläge, men ritar bara om när läget ändras
void updateDisplayMode() {
    DisplayMode wanted = hasBleReading ? MODE_NORMAL : MODE_OFF;
    if (wanted == displayMode) return;
    displayMode = wanted;
    if (isBlinking) return;   // ritas om när Whamageddon är slut
    if (wanted == MODE_OFF) renderOff(); else renderNormal();
}

// ---------------------------------------------------------------------------
// Sensorer och API:er
// ---------------------------------------------------------------------------
void checkCO2() {
    if (!scd4x.readMeasurement()) return;
    currentCO2 = scd4x.getCO2();
    hasCO2Reading = true;
    Serial.printf("CO2 Level: %u ppm\n", currentCO2);

    if (isBlinking || displayMode != MODE_NORMAL) return;
    setCO2LEDs(currentCO2);
    FastLED.show();
}

void updateHolfuyTemperature() {
    JsonDocument doc;
    if (!httpGetJson(holfuyUrl, doc)) return;

    if (!doc["temperature"].is<float>()) {
        Serial.println(F("Holfuy: temperature saknas i svaret"));
        return;
    }
    holfuyTemperature = doc["temperature"].as<float>();
    hasHolfuyReading = true;
    Serial.printf("Temp Holfuy: %.1f\n", holfuyTemperature);

    if (isBlinking || displayMode != MODE_NORMAL) return;
    setTempLEDs(2, holfuyTemperature);
    FastLED.show();
}

// Returnerar true om svaret var giltigt. "No song data" räknas som giltigt (ingen låt spelas).
bool fetchSongTitle(String& out) {
    JsonDocument filter;
    filter["TrackTitle"] = true;   // spara bara det fält vi behöver

    JsonDocument doc;
    if (!httpGetJson(songApiUrl, doc, &filter)) return false;

    const char* title = doc["TrackTitle"];
    out = (title && *title) ? String(title) : "No song data";
    return true;
}

void checkSong() {
    String current;
    if (!fetchSongTitle(current)) return;   // vid fel: behåll nuvarande läge
    lastSongTitle = current;
    Serial.println("Playing now: " + current);

    // Innehåller-matchning: "Last Christmas" matchar även "Last Christmas (Remastered)",
    // och en avkortad sparad titel (31 tecken) matchar fortfarande den fullständiga.
    String stored = normalizeTitle(readSongTitleFromEEPROM());
    bool match = stored.length() > 0 && normalizeTitle(current).indexOf(stored) >= 0;

    if (match && !isBlinking) {
        isBlinking = true;
        Serial.println(F("Whamageddon! Blinkar rött tills låten är slut."));
    } else if (!match && isBlinking) {
        isBlinking = false;
        Serial.println(F("Låten är slut, tillbaka till normalläge."));
        if (displayMode == MODE_OFF) renderOff(); else renderNormal();
    }
}

// ---------------------------------------------------------------------------
// BLE
// ---------------------------------------------------------------------------
bool caseInsensitiveEquals(const char* a, const char* b) {
    while (*a && *b) {
        if (tolower(*a) != tolower(*b)) return false;
        a++; b++;
    }
    return *a == *b;
}

// BTHome v2-parser (termometern kör pvvx-firmware i BTHome-läge).
// Letar upp temperaturen oavsett ordning på fälten i paketet.
// Körs i NimBLE-tasken: spara bara värdet, rör inte LED/display här.
void decodeServiceData(const std::string& p) {
    if (p.length() < 2 || ((uint8_t)p[0] & 0xE1) != 0x40) return; // BTHome v2, okrypterat

    size_t i = 1;
    while (i < p.length()) {
        uint8_t id = p[i++];
        uint8_t len;
        switch (id) {
            case 0x00: case 0x01: case 0x2E: case 0x0F: case 0x10: len = 1; break;
            case 0x02: case 0x03: case 0x0C: case 0x45:              len = 2; break;
            default: return;  // okänt objekt, kan inte veta längden
        }
        if (i + len > p.length()) return;

        if (id == 0x02 || id == 0x45) {   // temperatur: 0x02 = ×0.01, 0x45 = ×0.1
            int16_t raw = (uint8_t)p[i] | ((uint8_t)p[i + 1] << 8);
            bleTemperature = (id == 0x02) ? raw / 100.0 : raw / 10.0;
            bleTempPending = true;
            return;
        }
        i += len;
    }
}

// --- Felsökning: sätt till false när termometern fungerar ---
#define BLE_DEBUG true
volatile uint32_t bleAdvertsSeen = 0;

class ScanCallbacks : public NimBLEScanCallbacks {
    void onResult(const NimBLEAdvertisedDevice* dev) override {
        bleAdvertsSeen++;

#if BLE_DEBUG
        // Visa alla enheter som sänder BTHome (0xFCD2) eller pvvx/ATC-format (0x181A),
        // oavsett MAC – så syns termometern även om adressen ändrats
        for (uint8_t n = 0; n < dev->getServiceDataCount(); n++) {
            NimBLEUUID u = dev->getServiceDataUUID(n);
            if (u == NimBLEUUID((uint16_t)0xFCD2) || u == NimBLEUUID((uint16_t)0x181A)) {
                std::string sd = dev->getServiceData(n);
                Serial.printf("BLE-givare %s (RSSI %d) UUID %s:", dev->getAddress().toString().c_str(),
                              dev->getRSSI(), u.toString().c_str());
                for (uint8_t b : sd) Serial.printf(" %02X", b);
                Serial.println();
            }
        }
#endif

        if (!caseInsensitiveEquals(dev->getAddress().toString().c_str(), targetMacAddress.c_str())) return;

#if BLE_DEBUG
        Serial.printf("BLE: termometern hittad, RSSI %d, %d service data\n",
                      dev->getRSSI(), dev->getServiceDataCount());
        for (uint8_t n = 0; n < dev->getServiceDataCount(); n++) {
            std::string sd = dev->getServiceData(n);
            Serial.printf("  UUID %s:", dev->getServiceDataUUID(n).toString().c_str());
            for (uint8_t b : sd) Serial.printf(" %02X", b);
            Serial.println();
        }
#endif
        // Leta upp BTHome-datat (UUID 0xFCD2) bland eventuella service data
        static const NimBLEUUID bthomeUUID((uint16_t)0xFCD2);
        for (uint8_t n = 0; n < dev->getServiceDataCount(); n++) {
            if (dev->getServiceDataUUID(n) == bthomeUUID) {
                decodeServiceData(dev->getServiceData(n));
                return;
            }
        }
    }
};
ScanCallbacks scanCallbacks;

void handleBleReading() {
    if (bleTempPending) {
        bleTempPending = false;
        lastBleReadingMillis = millis();
        bool first = !hasBleReading;
        hasBleReading = true;
        Serial.printf("Temperature: %.2f °C\n", (float)bleTemperature);

        if (!first) {   // första värdet ritas av updateDisplayMode()
            displayCenteredText(String((int)round(bleTemperature)), 2, TEXT_Y);
            if (!isBlinking) {
                setTempLEDs(0, bleTemperature);
                FastLED.show();
            }
        }
    }

    // Sensorn har tystnat (t.ex. tomt batteri) -> tillbaka till OFF
    if (hasBleReading && millis() - lastBleReadingMillis > bleStaleTimeout) {
        Serial.println(F("Inga BLE-data på länge, visar OFF"));
        hasBleReading = false;
    }
}

// ---------------------------------------------------------------------------
// Webbserver
// ---------------------------------------------------------------------------
void handleSetSongTitle() {
    String songTitle = server.arg("songTitle");
    songTitle.trim();
    if (songTitle.length() > 0) {
        writeSongTitleToEEPROM(songTitle);
    }
    server.sendHeader("Location", "/");
    server.send(303);
}

void handleRoot() {
    String storedSongTitle = readSongTitleFromEEPROM();
    String temperatureString = hasBleReading ? String((float)bleTemperature, 1) + " °C" : "–";
    String holfuyString      = hasHolfuyReading ? String(holfuyTemperature, 1) + " °C" : "–";
    String co2String         = hasCO2Reading ? String(currentCO2) + " ppm" : "–";

    String html = R"rawliteral(<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Snowflake</title>
<style>
body{font-family:Arial,sans-serif;margin:0;padding:20px;background:#f4f4f9;color:#333;display:flex;flex-direction:column;align-items:center}
h1{color:#007bff;text-align:center}
p{margin:10px 0}
.container{max-width:600px;width:100%;background:#fff;padding:20px;border-radius:10px;box-shadow:0 2px 8px rgba(0,0,0,.1);box-sizing:border-box}
label{display:block;margin:10px 0 5px;font-weight:bold}
input[type=text]{width:100%;padding:10px;margin-bottom:15px;border:1px solid #ccc;border-radius:5px;box-sizing:border-box}
input[type=submit]{background:#007bff;color:#fff;border:none;padding:10px 20px;border-radius:5px;cursor:pointer;font-size:16px;width:100%}
input[type=submit]:hover{background:#0056b3}
a{display:block;text-align:center;margin-top:20px;color:#007bff;text-decoration:none;font-weight:bold}
a:hover{color:#0056b3}
.alert{color:#c00;font-weight:bold}
</style>
</head>
<body>
<div class="container">
<h1>Snowflake Lamp</h1>
<p><b>Local temperature: )rawliteral";
    html += temperatureString;
    html += F("</b></p>\n<p><b>Holfuy temperature: ");
    html += holfuyString;
    html += F("</b></p>\n<p><b>CO2 level: ");
    html += co2String;
    html += F("</b></p>\n<h2>Whamageddon warning - Mix Megapol</h2>\n<p>Playing now: ");
    html += htmlEscape(lastSongTitle);
    html += F("</p>\n<p>Warning for: ");
    html += htmlEscape(storedSongTitle);
    html += F("</p>\n");
    if (isBlinking) html += F("<p class=\"alert\">Låten spelas just nu!</p>\n");
    html += F("<form action=\"/setSongTitle\" method=\"post\">\n"
              "<label for=\"songTitle\">Set Song Title (max 31 tecken):</label>\n"
              "<input type=\"text\" id=\"songTitle\" name=\"songTitle\" maxlength=\"31\" value=\"");
    html += htmlEscape(storedSongTitle);
    html += F("\">\n<input type=\"submit\" value=\"Save\">\n</form>\n"
              "<a href=\"/update\">Update Firmware</a>\n</div>\n</body>\n</html>\n");

    server.send(200, "text/html", html);
}

// ---------------------------------------------------------------------------
// WiFi
// ---------------------------------------------------------------------------
void configModeCallback(WiFiManager* wm) {
    displayCenteredText("WiFi?", 2, TEXT_Y);
    Serial.println(F("Ingen WiFi – portal 'Snowflake' öppen i 3 min"));
}

void maintainWiFi() {
    if (WiFi.status() == WL_CONNECTED) return;
    if (millis() - previousWifiRetryMillis < wifiRetryInterval) return;
    previousWifiRetryMillis = millis();
    Serial.println(F("WiFi nere, försöker återansluta..."));
    WiFi.begin();   // sparade uppgifter
}

// ---------------------------------------------------------------------------
// setup / loop
// ---------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);

    // Display och LED först, så att något syns även utan WiFi
    Wire.begin(OLED_SDA, OLED_SCL);
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    displayCenteredText("HEJ", 2, TEXT_Y);

    FastLED.addLeds<WS2811, DATA_PIN, RGB>(leds, NUM_LEDS);
    FastLED.setBrightness(200);
    FastLED.setMaxPowerInVoltsAndMilliamps(5, 500);
    FastLED.clear(true);

    EEPROM.begin(512);

    // WiFi: portalen stängs efter 3 min så att lampan fungerar offline
    WiFiManager wifiManager;
    wifiManager.setAPCallback(configModeCallback);
    wifiManager.setConfigPortalTimeout(180);
    if (!wifiManager.autoConnect("Snowflake")) {
        Serial.println(F("Fortsätter utan WiFi"));
        WiFi.mode(WIFI_STA);
        WiFi.begin();
    }
    WiFi.setAutoReconnect(true);
    displayCenteredText("HEJ", 2, TEXT_Y);

    if (MDNS.begin(hostName)) {
        MDNS.addService("http", "tcp", 80);
    }

    server.on("/", HTTP_GET, handleRoot);
    server.on("/setSongTitle", HTTP_POST, handleSetSongTitle);
    ElegantOTA.begin(&server);
    server.begin();

    // CO2-sensor på egen I2C-buss
    Wire1.begin(CO2_SDA, CO2_SCL);
    if (!scd4x.begin(Wire1)) {
        Serial.println(F("Failed to initialize SCD4x sensor on custom I2C pins."));
    } else {
        scd4x.stopPeriodicMeasurement();
        scd4x.setSensorAltitude(0);
        scd4x.startPeriodicMeasurement();
    }

    // BLE
    NimBLEDevice::init("");
    pBLEScan = NimBLEDevice::getScan();
    pBLEScan->setScanCallbacks(&scanCallbacks, true);   // true = vill ha dubbletter
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);
    pBLEScan->setActiveScan(false);   // BTHome sänder allt i vanlig annonsering; passivt stör WiFi mindre
    pBLEScan->setMaxResults(0);                          // spara inga resultat, bara callback

    // Gör första koll direkt i stället för efter 45 s
    unsigned long now = millis();
    previousCO2CheckMillis   = now - sensorCheckInterval;
    previousHolfuyMillis     = now - sensorCheckInterval;
    previousTitleCheckMillis = now - sensorCheckInterval;
    previousBleScanMillis    = now - bleScanInterval;
}

void loop() {
    unsigned long now = millis();

    maintainWiFi();

    // Whamageddon-koll (oftare medan låten spelas)
    unsigned long titleInterval = isBlinking ? songCheckIntervalBlinking : sensorCheckInterval;
    if (now - previousTitleCheckMillis >= titleInterval) {
        previousTitleCheckMillis = now;
        checkSong();
    }

    handleBleReading();
    updateDisplayMode();

    if (now - previousCO2CheckMillis >= sensorCheckInterval) {
        previousCO2CheckMillis = now;
        checkCO2();
    }

    if (now - previousHolfuyMillis >= sensorCheckInterval) {
        previousHolfuyMillis = now;
        updateHolfuyTemperature();
    }

    // LED-animationer
    if (isBlinking) {
        blinkRedLEDs();
    } else if (displayMode == MODE_NORMAL) {
        updateCO2Alarm();
    }

    // BLE-scan, icke-blockerande (NimBLE 2.x: duration i ms)
    if (now - previousBleScanMillis >= bleScanInterval) {
        previousBleScanMillis = now;
        if (!pBLEScan->isScanning()) {
#if BLE_DEBUG
            Serial.printf("BLE: startar scan (%lu annonser sedan förra)\n", (unsigned long)bleAdvertsSeen);
            bleAdvertsSeen = 0;
#endif
            pBLEScan->start(bleScanDurationMs, false);
        }
    }

    ElegantOTA.loop();
    server.handleClient();
}
