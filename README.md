# Embedded World 2025 - Arduino Zigbee Demo

These 3 applications are designed for the ESP32-C6/H2 using Zigbee.

It is based on the ESP32 Arduino Zigbee Library that can be found at
https://github.com/espressif/arduino-esp32/tree/master/libraries/Zigbee

Each application uses [M5 Stack NanoC6](https://shop.m5stack.com/products/m5stack-nanoc6-dev-kit) dev kit and accessories connected to it:

- **Demo1:** Window Covering ([M5Stack Glass Unit w/ 1.51inch Transparent OLED](https://shop.m5stack.com/products/glass-unit-w-1-51inch-transparent-oled) with 2 Buttons)
- **Demo2:** Vibration sensor (SW-420)
- **Demo3:** CarbonDioxide (CO2), Temperature and Humidity SCD41 Sensor ([M5Stack CO2L](https://shop.m5stack.com/products/co2l-unit-with-temperature-and-humidity-sensor-scd41))

## Building the applications

Those 3 examples were built using Arduino IDE with ESP32 Arduino v3.1.3:

- https://github.com/espressif/arduino-esp32/releases/tag/3.1.3
- https://github.com/espressif/arduino-esp32/tree/release/v3.1.x

Demo1 acts as a Zigbee Router, Demo2 and Demo3 acts as Zigbee End Devices. In order to succesfully flash the code, proper Zigbee mode and Partition scheme must be configured in the Arduino IDE Tools menu.

| Zigbee role | Zigbee mode | Partition scheme |
| ----------- | ----------- | ---------------- |
| End device | Zigbee ED | Zigbee 2/4/8 MB with SPIFFS |
| Router/Coordinator | Zigbee ZCZR | Zigbee ZCZR 2/4/8 MB with SPIFFS |

It is strongly recommended that the flash is erased before uploading in order to avoid using previous NVS Zigbee stored data.
Some useful log information may be seen in the Console UART0 when using Core Debug Level as Info or Verbose.
