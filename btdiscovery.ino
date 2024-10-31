#include <NimBLEDevice.h>

const uint16_t BTHOME_SERVICE_UUID = 0xFCD2; // BTHome UUID for v2 format
const uint8_t TEMPERATURE_DATA_ID = 0x02;    // Temperature ID as per BTHome v2 format

void setup() {
  Serial.begin(115200);
  NimBLEDevice::init("");   // Initialize BLE, no device name needed as we're just scanning
  NimBLEDevice::setScanFilterMode(CONFIG_BTDM_SCAN_DUPL_TYPE_DATA); // Only new data
  
  NimBLEScan* pBLEScan = NimBLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true); // Active scan to get data content
  pBLEScan->start(5, nullptr, false); // Start scanning
}

void loop() {
  // Keep scanning every 5 seconds
  NimBLEDevice::getScan()->start(5, nullptr, false);
  delay(1000);
}

// Callback handler for each BLE device detected
class MyAdvertisedDeviceCallbacks : public NimBLEAdvertisedDeviceCallbacks {
  void onResult(NimBLEAdvertisedDevice* advertisedDevice) {
    // Check if this is a BTHome device by service UUID
    if (advertisedDevice->haveServiceUUID() && advertisedDevice->getServiceUUID().equals(NimBLEUUID(BTHOME_SERVICE_UUID))) {
      Serial.println("BTHome device found!");
      
      // Extract advertisement data
      std::string advData = advertisedDevice->getManufacturerData();
      
      if (!advData.empty() && advData.length() > 2) {
        // Parse the data for temperature
        parseBTHomeData((uint8_t*)advData.data(), advData.length());
      }
    }
  }
};

// Function to parse BTHome v2 advertisement data and extract temperature
void parseBTHomeData(uint8_t* data, size_t length) {
  for (size_t i = 0; i < length; i++) {
    if (data[i] == TEMPERATURE_DATA_ID) {  // Check for temperature ID in BTHome format
      if (i + 2 < length) {  // Ensure there's enough data for the temperature value
        int16_t rawTemperature = (data[i + 1] | (data[i + 2] << 8)); // Combine bytes
        float temperature = rawTemperature / 100.0; // Convert to Celsius
        Serial.print("Temperature: ");
        Serial.print(temperature);
        Serial.println(" °C");
      }
      break; // Exit after finding temperature
    }
  }
}
