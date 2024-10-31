#include <NimBLEDevice.h>
#include "ATC_MiThermometer.h"

const char* deviceAddress = "A4:C1:38:XX:XX:XX"; // MAC address of the thermometer

ATC_MiThermometer thermometer(deviceAddress, Connection_mode::ADVERTISING);

void setup() {
  Serial.begin(115200);
  NimBLEDevice::init("");

  // Initialize the thermometer
  thermometer.init();
}
void loop() {
  // Reading temperature
  float temperature = thermometer.getTemperature();
  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.println(" °C");

  // Reading humidity
  float humidity = thermometer.getHumidity();
  Serial.print("Humidity: ");
  Serial.print(humidity);
  Serial.println(" %");

  // Reading battery level
  uint8_t batteryLevel = thermometer.getBatteryLevel();
  Serial.print("Battery Level: ");
  Serial.print(batteryLevel);
  Serial.println(" %");

  delay(5000); // Wait 5 seconds before next reading
}
