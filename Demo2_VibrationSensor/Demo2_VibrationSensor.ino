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
// Description:
// This example demonstrates usage of a vibration sensor with Zigbee. Vibration sensor used is SW-420, which is a simple vibration sensor that can be used to detect vibrations and shocks.
// The sensor is connected to the ESP32-C6/H2 and the ZigbeeVibrationSensor class is used to create a Zigbee endpoint for the vibration sensor. 
// The class provides functions to set the vibration status of the sensor. There is a 2-second delay between each vibration sensing, to avoid false positives 
// and to hold the vibration status for a longer time.
// To showcase the OTA firmware update feature, this example add
//
// Proper Zigbee mode must be selected in Tools->Zigbee mode->Zigbee ED (End Device)
// and also the correct partition scheme must be selected in Tools->Partition Scheme->Zigbee 4MB (or 2MB / 8MB) 
//
// Created by Jan Procházka (https://github.com/P-R-O-C-H-Y/)


#ifndef ZIGBEE_MODE_ED
#error "Zigbee end device mode is not selected in Tools->Zigbee mode"
#endif

#include "Zigbee.h"

/* Zigbee vibration sensor configuration */
#define VIBRATION_SENSOR_ENDPOINT_NUMBER 10

/* Zigbee OTA configuration */
#define OTA_UPGRADE_RUNNING_FILE_VERSION    0x01010100  // Increment this value when the running image is updated
#define OTA_UPGRADE_DOWNLOADED_FILE_VERSION 0x01010101  // Increment this value when the downloaded image is updated
#define OTA_UPGRADE_HW_VERSION              0x0101      // The hardware version, this can be used to differentiate between different hardware versions

uint8_t button = BOOT_PIN;
uint8_t sensor_pin = 4;

ZigbeeVibrationSensor zbVibrationSensor = ZigbeeVibrationSensor(VIBRATION_SENSOR_ENDPOINT_NUMBER);

void setup() {
  Serial.begin(115200);

  // Init button + sensor
  pinMode(button, INPUT_PULLUP);
  pinMode(sensor_pin, INPUT);

  // Optional: set Zigbee device name and model
  zbVibrationSensor.setManufacturerAndModel("Espressif", "ZigbeeVibrationSensor");

  // Add OTA client to the vibration sensor
  zbVibrationSensor.addOTAClient(OTA_UPGRADE_RUNNING_FILE_VERSION, OTA_UPGRADE_DOWNLOADED_FILE_VERSION, OTA_UPGRADE_HW_VERSION);

  // Add endpoint to Zigbee Core
  Zigbee.addEndpoint(&zbVibrationSensor);

  Serial.println("Starting Zigbee...");
  // When all EPs are registered, start Zigbee in End Device mode
  if (!Zigbee.begin()) {
    Serial.println("Zigbee failed to start!");
    Serial.println("Rebooting...");
    ESP.restart();
  } else {
    Serial.println("Zigbee started successfully!");
  }
  Serial.println("Connecting to network");
  while (!Zigbee.connected()) {
    Serial.print(".");
    delay(100);
  }
  Serial.println();

  // Start Zigbee OTA client query, first request is within a minute and the next requests are sent every hour automatically
  zbVibrationSensor.requestOTAUpdate();
}

void loop() {
  // Checking pin for contact change
  static bool sensed = false;
  if (digitalRead(sensor_pin) == HIGH && !sensed) {
    // Update contact sensor value
    zbVibrationSensor.setVibration(true);
    sensed = true;
    //if sensed, wait 2 seconds before next sensing
    delay(2000);
  } else if (digitalRead(sensor_pin) == LOW && sensed) {
    zbVibrationSensor.setVibration(false);
    sensed = false;
    //if not sensed, wait 0,5 seconds before next sensing
    delay(500);
  }

  // Checking button for factory reset
  if (digitalRead(button) == LOW) {  // Push button pressed
    // Key debounce handling
    delay(100);
    int startTime = millis();
    while (digitalRead(button) == LOW) {
      delay(50);
      if ((millis() - startTime) > 3000) {
        // If key pressed for more than 3secs, factory reset Zigbee and reboot
        Serial.println("Resetting Zigbee to factory and rebooting in 1s.");
        delay(1000);
        Zigbee.factoryReset();
      }
    }
  }
  delay(100);
}
