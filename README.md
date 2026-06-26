# Match-The-Shape_Arduino
A reaction game running on an ESP32 microcontroller, displayed on a 128x64 OLED screen.

A random shape appears on the screen — triangle, circle, or square — and you have 60 seconds to press the correct matching button as many times as possible. Press the wrong button and you lose a point.

## How it works

Each button corresponds to a shape. When the correct button is pressed, the shape changes to a new random one. Wrong button presses deduct 1 from your score (minimum 0). The game ends after 60 seconds and displays your final score.

## Hardware

- ESP32
- 128x64 OLED display (I2C)
- 3 push buttons (pins 23, 19, 18 — make sure to change the button definitions to fit the wiring of your boards)

## Libraries

- **U8g2** — for driving the OLED display
- **Wire** — for I2C communication

