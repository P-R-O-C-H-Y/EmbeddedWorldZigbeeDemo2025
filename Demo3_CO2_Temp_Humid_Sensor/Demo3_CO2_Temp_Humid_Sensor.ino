// Copyright 2025 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Dependencies:
// https://github.com/Sensirion/arduino-i2c-scd4x
//
// Description:
// This example demonstrates usage of a temperature, humidity and CO2 sensor with Zigbee. The sensor used is Sensirion SCD41, 
// which is a high-precision sensor for measuring CO2, temperature and humidity.
// The sensor is connected to the ESP32-C6/H2 and the ZigbeeTempSensor and ZigbeeCarbonDioxideSensor classes are used to create Zigbee endpoints for the sensor.
// The classes provide functions to set the temperature, humidity and CO2 values of the sensor. The sensor is put to sleep for 55 seconds and then wakes up to measure and report the values.
// The sensor is put to sleep to save power to showcase the sleepy end device feature of Zigbee.
// A battery level of 100% is set for demonstration purposes. The battery level can be updated by calling zbTempSensor.setBatteryPercentage(percentage) anytime.
// The LED on the ESP32-C6/H2 is turned on on device wake up and turned off before going to sleep, to indicate the device status.
//
// Proper Zigbee mode must be selected in Tools->Zigbee mode->Zigbee ED (End Device)
// and also the correct partition scheme must be selected in Tools->Partition Scheme->Zigbee 4MB (or 2MB / 8MB) 
//
// Created by Jan Procházka (https://github.com/P-R-O-C-H-Y/)

#ifndef ZIGBEE_MODE_ED
#error "Zigbee end device mode is not selected in Tools->Zigbee mode"
#endif

#include <Wire.h>
#include <SensirionI2cScd4x.h>
#include "Zigbee.h"

/* Zigbee temperature/humidity, co2 sensor configuration */
#define TEMP_SENSOR_ENDPOINT_NUMBER 1
#define CO2_SENSOR_ENDPOINT_NUMBER 2

#define uS_TO_S_FACTOR 1000000ULL /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  55         /* Sleep for 55s will + 5s delay for establishing connection => data reported every 1 minute */

#define I2C_SDA 2
#define I2C_SCL 1

const uint8_t buttonPin = 9;
const uint8_t ledPin = 7;

static int16_t error;
static char errorMessage[64];

SensirionI2cScd4x sensor;

ZigbeeTempSensor zbTempSensor = ZigbeeTempSensor(TEMP_SENSOR_ENDPOINT_NUMBER);
ZigbeeCarbonDioxideSensor zbCO2Sensor = ZigbeeCarbonDioxideSensor(CO2_SENSOR_ENDPOINT_NUMBER);

/********************* Arduino functions **************************/
void setup() {
  Serial.begin(115200);

  // Init button switch
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, HIGH);

  // Init SCD41 sensor
  Wire.setPins(I2C_SDA, I2C_SCL);
  Wire.begin();
  sensor.begin(Wire, SCD41_I2C_ADDR_62);
  sensorInit();

  // Configure the wake up source and set to wake up every 5 seconds
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);

  // Init temperature sensor
  zbTempSensor.setManufacturerAndModel("Espressif", "ZigbeeSCD41");
  zbTempSensor.setMinMaxValue(-10, 60);
  zbTempSensor.setTolerance(0.8);
  zbTempSensor.addHumiditySensor(0, 95, 6);

  // Init CO2 sensor
  zbCO2Sensor.setManufacturerAndModel("Espressif", "ZigbeeSCD41");
  zbCO2Sensor.setMinMaxValue(400, 5000);

  // Set power source to battery and set battery percentage to measured value (now 100% for demonstration)
  // The value can be also updated by calling zbTempSensor.setBatteryPercentage(percentage) anytime
  zbTempSensor.setPowerSource(ZB_POWER_SOURCE_BATTERY, 100);

  // Add endpoint to Zigbee Core
  Zigbee.addEndpoint(&zbTempSensor);
  Zigbee.addEndpoint(&zbCO2Sensor);

// Create a custom Zigbee configuration for End Device with keep alive 10s to avoid interference with reporting data
  esp_zb_cfg_t zigbeeConfig = ZIGBEE_DEFAULT_ED_CONFIG();
  zigbeeConfig.nwk_cfg.zed_cfg.keep_alive = 10000;

  // When all EPs are registered, start Zigbee in End Device mode
  if (!Zigbee.begin(&zigbeeConfig, false)) {
    Serial.println("Zigbee failed to start!");
    Serial.println("Rebooting...");
    ESP.restart();
  }
  Serial.println("Connecting to network");
  while (!Zigbee.connected()) {
    Serial.print(".");
    delay(100);
  }
  Serial.println();
  Serial.println("Successfully connected to Zigbee network");

  // Delay approx 1s (may be adjusted) to allow establishing proper connection with coordinator, needed for sleepy devices
  delay(1000);
}

void loop() {
  // Checking button for factory reset
  if (digitalRead(buttonPin) == LOW) {  // Push button pressed
    // Key debounce handling
    delay(100);
    int startTime = millis();
    while (digitalRead(buttonPin) == LOW) {
      delay(50);
      if ((millis() - startTime) > 3000) {
        // If key pressed for more than 3secs, factory reset Zigbee and reboot
        Serial.println("Resetting Zigbee to factory and rebooting in 1s.");
        delay(1000);
        digitalWrite(ledPin, LOW);
        Zigbee.factoryReset();
      }
    }
  }

  // Call meaurement and report
  meausureAndReport();

  // Put the device to sleep
  Serial.println("Going to sleep now");
  digitalWrite(ledPin, LOW);
  esp_deep_sleep_start();
}

/*************************** SCD41 sensor ***************************/
void PrintUint64(uint64_t& value) {
    Serial.print("0x");
    Serial.print((uint32_t)(value >> 32), HEX);
    Serial.print((uint32_t)(value & 0xFFFFFFFF), HEX);
}

void sensorInit(){
    uint64_t serialNumber = 0;
    delay(30);
    // Ensure sensor is in clean state
    error = sensor.wakeUp();
    if (error != 0) {
        Serial.print("Error trying to execute wakeUp(): ");
        errorToString(error, errorMessage, sizeof errorMessage);
        Serial.println(errorMessage);
    }
    error = sensor.stopPeriodicMeasurement();
    if (error != 0) {
        Serial.print("Error trying to execute stopPeriodicMeasurement(): ");
        errorToString(error, errorMessage, sizeof errorMessage);
        Serial.println(errorMessage);
    }
    error = sensor.reinit();
    if (error != 0) {
        Serial.print("Error trying to execute reinit(): ");
        errorToString(error, errorMessage, sizeof errorMessage);
        Serial.println(errorMessage);
    }
    // Read out information about the sensor
    error = sensor.getSerialNumber(serialNumber);
    if (error != 0) {
        Serial.print("Error trying to execute getSerialNumber(): ");
        errorToString(error, errorMessage, sizeof errorMessage);
        Serial.println(errorMessage);
        return;
    }
    Serial.print("serial number: 0x");
    PrintUint64(serialNumber);
    Serial.println();
}

void meausureAndReport() {
    uint16_t co2Concentration = 0;
    float temperature = 0.0;
    float relativeHumidity = 0.0;
    //
    // Wake the sensor up from sleep mode.
    //
    error = sensor.wakeUp();
    if (error != 0) {
        Serial.print("Error trying to execute wakeUp(): ");
        errorToString(error, errorMessage, sizeof errorMessage);
        Serial.println(errorMessage);
        return;
    }
    //
    // Ignore first measurement after wake up.
    //
    error = sensor.measureSingleShot();
    if (error != 0) {
        Serial.print("Error trying to execute measureSingleShot(): ");
        errorToString(error, errorMessage, sizeof errorMessage);
        Serial.println(errorMessage);
        return;
    }
    //
    // Perform single shot measurement and read data.
    //
    error = sensor.measureAndReadSingleShot(co2Concentration, temperature,
                                            relativeHumidity);
    if (error != 0) {
        Serial.print("Error trying to execute measureAndReadSingleShot(): ");
        errorToString(error, errorMessage, sizeof errorMessage);
        Serial.println(errorMessage);
        return;
    }
    //
    // Print results in physical units.
    //
    Serial.print("CO2 concentration [ppm]: ");
    Serial.print(co2Concentration);
    Serial.println();
    Serial.print("Temperature [°C]: ");
    Serial.print(temperature);
    Serial.println();
    Serial.print("Relative Humidity [RH]: ");
    Serial.print(relativeHumidity);
    Serial.println();

  // Update temperature and humidity values in Temperature sensor EP
  zbTempSensor.setTemperature(temperature);
  zbTempSensor.setHumidity(relativeHumidity);

  // Update CO2 value in CO2 sensor EP
  zbCO2Sensor.setCarbonDioxide(co2Concentration);

  // Report temperature, humidity and CO2 values
  zbTempSensor.report();
  zbCO2Sensor.report();

  // Add small delay to allow the data to be sent before going to sleep
  delay(200);
}
