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
// https://github.com/m5stack/M5Unit-GLASS
//
// Description:
// This example demonstrates how to control a window covering using Zigbee. The window covering is simulated by a set of blinds that can be lifted and tilted. 
// The blinds are controlled by a set of buttons on the M5Unit-Glass. The position of the blinds is displayed on the screen. 
// The blinds can also be controlled using Zigbee commands. The position of the blinds is updated on the screen and the Zigbee commands are printed on the serial monitor.
// The LED of the ESP32-C6/H2 is turned on when the blinds are moving and turned off when the blinds are stopped.
//
// The example uses the ZigbeeWindowCovering class to create a Zigbee endpoint for the window covering. The class provides functions to open, close, and move the blinds to a specific position.
//
// Proper Zigbee mode must be selected in Tools->Zigbee mode->Zigbee ZCZR (Coordinator/Router)
// and also the correct partition scheme must be selected in Tools->Partition Scheme->Zigbee ZCZR 4MB (or 2MB / 8MB) 
//
// Created by Jan Procházka (https://github.com/P-R-O-C-H-Y/)

#ifndef ZIGBEE_MODE_ZCZR
#error "Zigbee coordinator/router mode is not selected in Tools->Zigbee mode"
#endif

#include <Wire.h>
#include <UNIT_GLASS.h>
#include "Zigbee.h"

// OLED display setup
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1  

#define I2C_SDA 2
#define I2C_SCL 1

#define WHITE 0   // Color for drawing

UNIT_GLASS Glass;

// Zigbee setup
#define ZIGBEE_COVERING_ENDPOINT 1
#define BUTTON_PIN               9  // ESP32-C6/H2 Boot button

#define MAX_LIFT 9  // centimeters from open position (0-900)
#define MIN_LIFT 0

#define MAX_TILT 5  // centimeters from open position (0-900)
#define MIN_TILT 0

const uint8_t buttonPin = 9;
const uint8_t ledPin = 7;

uint16_t currentLift = MAX_LIFT;
uint8_t currentLiftPercentage = 100;

uint16_t currentTilt = MAX_TILT;
uint8_t currentTiltPercentage = 100;

bool stop = false; // Stop the motor
bool moving = false; // Motor is moving

int16_t nextLift = -1;
int16_t nextTilt = -1;

ZigbeeWindowCovering zbCovering = ZigbeeWindowCovering(ZIGBEE_COVERING_ENDPOINT);

void setup() {
    pinMode(buttonPin, INPUT_PULLUP);
    pinMode(ledPin, OUTPUT);
    digitalWrite(ledPin, LOW);

    Serial.begin(115200);

    /* Unit-Glass init */
    Glass.begin(&Wire, GLASS_ADDR, I2C_SDA, I2C_SCL, 100000UL);
    Glass.invert(0);
    Glass.color_invert(0);
    Glass.clear();

    // Draw the initial blinds
    drawBlinds();

    // Zigbee setup
    zbCovering.setManufacturerAndModel("Espressif", "WindowBlinds");
    zbCovering.setCoveringType(BLIND_LIFT_AND_TILT);
    // Set configuration: operational, online, not commands_reversed, lift / tilt closed_loop, lift / tilt encoder_controlled
    zbCovering.setConfigStatus(true, true, false, true, true, true, true);
    // Set mode: not motor_reversed, calibration_mode, not maintenance_mode, not leds_on
    zbCovering.setMode(false, false, false, false);
    // Set limits of motion
    zbCovering.setLimits(MIN_LIFT, MAX_LIFT, MIN_TILT, MAX_TILT);
    // Set callback function for open, close, filt and tilt change, stop
    zbCovering.onOpen(fullOpen);
    zbCovering.onClose(fullClose);
    zbCovering.onGoToLiftPercentage(goToLiftPercentage);
    zbCovering.onGoToTiltPercentage(goToTiltPercentage);
    zbCovering.onStop(stopMotor);

    // Add endpoint to Zigbee Core
    Serial.println("Adding ZigbeeWindowCovering endpoint to Zigbee Core");
    Zigbee.addEndpoint(&zbCovering);

    // When all EPs are registered, start Zigbee in router mode
    Serial.println("Calling Zigbee.begin()");
    if (!Zigbee.begin(ZIGBEE_ROUTER)) {
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

    // Set initial position
    zbCovering.setLiftPercentage(currentLiftPercentage);
    zbCovering.setTiltPercentage(currentTiltPercentage);
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
                Zigbee.factoryReset();
            }
        }
    }

    if (Glass.getKeyA() == 0 && currentLift > MIN_LIFT) {
        nextLift = currentLift-1;
    }
    if (Glass.getKeyB() == 0 && currentLift < MAX_LIFT) {
        nextLift = currentLift+1;
    }

    if(nextLift != -1) {
        moveCoversLift(nextLift);
        nextLift = -1;
    }
    if(nextTilt != -1) {
        moveCoversTilt(nextTilt);
        nextTilt = -1;
    }
}

/************** ZIGBEE FUNCTIONS **************/

void fullOpen() {
  Serial.println("Opening cover");
  nextLift = MIN_LIFT;
}

void fullClose() {
  Serial.println("Closing cover");
  nextLift = MAX_LIFT;
}

void goToLiftPercentage(uint8_t liftPercentage) {
  uint16_t lift_position = (liftPercentage * MAX_LIFT) / 100;
  Serial.printf("New requested lift from Zigbee: %d (%d)\n", lift_position, liftPercentage);
  nextLift = lift_position;
}

void goToTiltPercentage(uint8_t tiltPercentage) {
  uint16_t tilt_position = (tiltPercentage * MAX_TILT) / 100;
  Serial.printf("New requested tilt from Zigbee: %d (%d)\n", tilt_position, tiltPercentage);
  nextTilt = tilt_position;
}

void stopMotor() {
  if(moving) {
    stop = true;
    Serial.println("Stopping motor");
  }
}

void moveCoversLift(uint16_t lift_position){
    if (lift_position == currentLift) {
        return;
    }   
    moving = true;
    digitalWrite(ledPin, HIGH);
    int step = (currentLift < lift_position) ? 1 : -1;
    for (int i = currentLift+step; i != lift_position+step; i += step) {
        if(stop) {
            stop = false;
            moving = false;
            break;
        }
        currentLift = i;
        drawBlinds();
        currentLiftPercentage = (currentLift * 100) / MAX_LIFT;
        // Update the current position
        zbCovering.setLiftPercentage(currentLiftPercentage);
        Serial.printf("Lifting to %d\n", currentLift);
        delay(250);
    }
    moving = false;
    digitalWrite(ledPin, LOW);
}

void moveCoversTilt(uint16_t tilt_position){
    if (tilt_position == currentTilt) {
        return;
    } 
    moving = true; 
    digitalWrite(ledPin, HIGH);
    int step = (currentTilt < tilt_position) ? 1 : -1;
    for (int i = currentTilt+step; i != tilt_position+step; i += step) {
        if(stop) {
            stop = false;
            moving = false;
            break;
        }
        currentTilt = i;
        drawBlinds();
        currentTiltPercentage = (currentTilt * 100) / MAX_LIFT;
        // Update the current position
        zbCovering.setTiltPercentage(currentTiltPercentage);
        Serial.printf("Tilting to %d\n", currentTilt);
        delay(250);
    }
    moving = false;
    digitalWrite(ledPin, LOW);
}

/************** DISPLAY FUNCTIONS **************/
// Function to draw blinds while keeping the top lines fixed
void drawBlinds() {
    Glass.clear();

    int y_start = 0;
    int base_slat_height = 6; 
    int spacing = 1;  
    int max_thickness = base_slat_height;  

    for (int i = 0; i < currentLift; i++) {  // Adjust slats based on position
        int y = y_start + (i * (max_thickness + spacing)); // Keep top line fixed
        int thickness = 1 + currentTilt;
        drawSlat(10, y, thickness);
    }

    drawManualController();  
    Glass.show();
}

// Function to draw a slat while keeping its top line fixed    
void drawSlat(int x, int y, int thickness) {
    int width = 100;

    // Always draw the top line
    Glass.draw_line(x, y, x + width, y, 0xFF);
    
    // Draw the lower portion based on tilt
    if (thickness > 0) {
        // Glass.fill_rect(x, y + 1, width, thickness - 1, WHITE);
        // Glass.draw_picture(x, y + 1, width, thickness - 1, WHITE);
        //use draw_line instead of fill_rect
        for(int i = 0; i < thickness - 1; i++) {
            Glass.draw_line(x, y + i + 1, x + width, y + i + 1, 0xFF);
        }
    }
}

// Draws a manual control wire on the side
void drawManualController() {
    int wire_x = 120;  
    int wire_y_start = 0;  
    int wire_y_end = 58;  
    int knob_y_end = 50;

    Glass.draw_line(wire_x, wire_y_start, wire_x, wire_y_end, 0xFF); 

    static int knob_y = wire_y_start;
    int new_knob_y = map(currentLift, 0, MAX_LIFT, wire_y_start, knob_y_end);
    
    if (knob_y != new_knob_y) {  
        knob_y = new_knob_y;
    }

    // Glass.draw_circle(wire_x, knob_y, 3, WHITE);  
    // use draw_picture instead a circle
    Glass.draw_picture(wire_x, knob_y, 3, 3, 0);
}
