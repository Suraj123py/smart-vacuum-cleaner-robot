# Smart Vacuum Cleaner Robot

An autonomous vacuum cleaner robot built around the ESP32-S3 EYE, with obstacle
and cliff detection, a camera, and web-based control.

## Hardware
- ESP32-S3 EYE
- L298N motor driver
- 3x HC-SR04 ultrasonic sensors
- IR cliff sensor
- OV2640 camera
- ESC-controlled vacuum fan
- INMP441 I2S microphone

## Software
- Arduino framework for ESP32
- FreeRTOS dual-core design: robot logic on Core 0, HTTP server on Core 1
- Non-blocking sensor reads using millis()
- WebServer.h with the WebSockets library by Markus Sattler

## Board settings (Arduino IDE)
- Board: ESP32S3 Dev Module
- PSRAM: OPI PSRAM
- Partition scheme: Huge APP (3MB No OTA)
- CPU frequency: 240 MHz
- USB CDC On Boot: Enabled

## Setup
1. Install the ESP32 board package in Arduino IDE
2. Install the WebSockets library by Markus Sattler
3. Open the firmware folder and set your WiFi name and password in the code
4. Select the board settings above and upload

## Challenges solved
- Web server responsiveness
- IR sensor logic inversion
- ESC calibration triggering unexpectedly
- CPU starvation, solved with the dual-core FreeRTOS design
- Compatibility with ESP32 Arduino Core v3.x


