# Kompilerade med Visual Studio Platformio för att få det att funka
# Detta behövs i din platformio.ini
# Detta med en liten esp32c4 https://www.aliexpress.com/item/1005006960134338.html
#
# [env:seeed_xiao_esp32c3]
# platform = espressif32
# board = seeed_xiao_esp32c3
# framework = arduino
# lib_deps = 
# 	h2zero/NimBLE-Arduino@^1.4.2
# 	fastled/FastLED@^3.9.4





#include <NimBLEDevice.h>
#include <FastLED.h>

// BLE Scan settings
NimBLEScan* pBLEScan;
const int scanTime = 5;  // Scan duration in seconds

// FastLED settings
#define LED_PIN 4
#define NUM_LEDS 1
CRGB leds[NUM_LEDS];

// Target MAC address of the specific device
const std::string targetMacAddress = "34:ec:b6:65:16:fb";  // Update as needed

// Helper function to decode a little-endian 16-bit unsigned integer
uint16_t decodeLittleEndianU16(uint8_t lowByte, uint8_t highByte) {
  return (uint16_t)((highByte << 8) | lowByte);
}

// Helper function to print the raw service data
void printRawServiceData(const std::string& payload) {
  Serial.print("Raw Service Data: ");
  for (size_t i = 0; i < payload.length(); ++i) {
    Serial.printf("%02X ", static_cast<uint8_t>(payload[i]));
  }
  Serial.println();
}

// Set LED color based on temperature
void setLEDColor(float roundedTemperature) {
  Serial.print("Setting LED color based on temperature: ");
  Serial.println(roundedTemperature);

  if (roundedTemperature >= -50 && roundedTemperature < -5) {
    leds[0] = 0xFF44DD;  // Pink
  } else if (roundedTemperature >= -5 && roundedTemperature < 0) {
    leds[0] = 0x0006FF;  // Blue
  } else if (roundedTemperature >= 0 && roundedTemperature <= 5) {
    leds[0] = 0x00FF06;  // Green
  } else if (roundedTemperature > 5 && roundedTemperature <= 10) {
    leds[0] = 0xFFF600;  // Yellow
  } else if (roundedTemperature > 10 && roundedTemperature <= 15) {
    leds[0] = 0xFFA500;  // Orange
  } else if (roundedTemperature > 15 && roundedTemperature <= 20) {
    leds[0] = 0xFF0000;  // Red
  } else if (roundedTemperature > 20 && roundedTemperature <= 45) {
    leds[0] = 0x800080;  // Purple
  } else {
    leds[0] = 0x000000;  // Off
  }

  FastLED.show();
}

// Function to parse and decode the raw service data
void decodeServiceData(const std::string& payload) {
  // Print the raw service data for debugging
  printRawServiceData(payload);

  // Ensure we have at least 14 bytes for the expected data
  if (payload.length() >= 14) {
    // Check if the first byte indicates a valid service data format
    if (payload[0] == 0x40) {
      // Parse BTHome device info (flags and version)
      uint8_t flags = payload[0];  // First byte (flag byte)
      uint8_t bthomeVersion = (flags >> 5) & 0x07;  // BTHome Version is in bits 5-7
      Serial.print("BTHome Version: ");
      Serial.println(bthomeVersion);

      // Decode temperature (bytes 6 and 7) in little-endian format
      int16_t temp_int = (payload[6] & 0xff) | ((payload[7] & 0xff) << 8);
      float temperature = temp_int / 100.0;
      float roundedTemperature = round(temperature);
      Serial.print("Temperature: ");
      Serial.print(temperature);
      Serial.println(" °C");

      // Set LED color based on rounded temperature
      setLEDColor(roundedTemperature);

      // Decode humidity (bytes 9 and 10) in little-endian format
      float humidity = ((payload[9] & 0xff) | ((payload[10] & 0xff) << 8)) / 100.0;
      Serial.print("Humidity: ");
      Serial.print(humidity);
      Serial.println(" %");

      // Decode voltage (bytes 12 and 13) in little-endian format
      float voltage = ((payload[12] & 0xff) | ((payload[13] & 0xff) << 8)) / 1000.0;
      Serial.print("Voltage: ");
      Serial.println(voltage);
    } else {
      Serial.println("Invalid Service Data format.");
    }
  } else {
    Serial.println("Unexpected payload length or structure.");
  }
}

// Callback to handle BLE Advertisements
class MyAdvertisedDeviceCallbacks : public NimBLEAdvertisedDeviceCallbacks {
  void onResult(NimBLEAdvertisedDevice* advertisedDevice) {
    // Check if the device's MAC address matches the target address
    if (advertisedDevice->getAddress().toString() == targetMacAddress) {
      Serial.print("Found target device: ");
      Serial.println(advertisedDevice->getAddress().toString().c_str());

      // Check if the device has service data
      if (advertisedDevice->haveServiceData()) {
        std::string serviceData = advertisedDevice->getServiceData();
        Serial.print("Service Data Length: ");
        Serial.println(serviceData.length());
        decodeServiceData(serviceData);
      } else {
        Serial.println("No service data available.");
      }
    }
  }
};

void setup() {
  Serial.begin(115200);
  Serial.println("Starting BLE Scanner...");

  // Initialize FastLED
  FastLED.addLeds<WS2812, LED_PIN, RGB>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  leds[0] = 0x000000;  // Start with LED off
  FastLED.show();

  // Initialize BLE
  NimBLEDevice::init("");
  pBLEScan = NimBLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks(), true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);
  pBLEScan->setActiveScan(true);
}

void loop() {
  Serial.println("Scanning for BLE devices...");
  pBLEScan->start(scanTime, false);  // Scan for `scanTime` seconds
  delay(2000);  // Wait before the next scan
}
