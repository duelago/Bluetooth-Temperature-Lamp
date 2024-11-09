#include <NimBLEDevice.h>

// BLE Scan settings
NimBLEScan* pBLEScan;
const int scanTime = 5;  // Scan duration in seconds

// Target MAC address of the specific device
const std::string targetMacAddress = "34:ec:b6:65:18:3e";

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
      Serial.print("Temperature: ");
      Serial.print(temperature);
      Serial.println(" °C");

      // Decode humidity (bytes 9 and 10) in little-endian format
      float humidity = ((payload[9] & 0xff) | ((payload[10] & 0xff) << 8)) / 100.0;
      Serial.print("Humidity: ");
      Serial.print(humidity);
      Serial.println(" %");

      // Decode voltage (bytes 12 and 13) in little-endian format
      float voltage = ((payload[12] & 0xff) | ((payload[13] & 0xff) << 8)) / 1000.0;
      Serial.print("Voltage: ");
      Serial.print(voltage);
      Serial.println(" V");

      // Check validity of decoded values based on ESPHome's conditions
      if (payload[5] == 0x02 && payload[8] == 0x03 && payload[11] == 0x0C &&
          temperature > -40 && temperature < 100 && humidity > 0 && humidity < 101 && voltage > 0 && voltage < 4) {
        
        // Publish the values (you can replace these with your actual publish method)
        Serial.print("Publishing Data: ");
        Serial.print("Temperature: ");
        Serial.print(temperature);
        Serial.print(" Humidity: ");
        Serial.print(humidity);
        Serial.print(" Voltage: ");
        Serial.println(voltage);
        
        // Here, replace with your own logic to publish the data
        // id(BTH01_temperature_).publish_state(temperature);
        // id(BTH01_humidity_).publish_state(humidity);
        // id(BTH01_volt_).publish_state(voltage);
      }
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
