# Hardware

Hardware configuration and wiring information for the InjectCoach prototype.

## Main Components

- ESP32 Dev Module
- MPU6050
- FSR-402
- Plunger button
- Vibration motor module
- Active buzzer
- LEDs
- Silicone pad
- Mock injection pen

## Current Pin Assignment

| Function | ESP32 Pin |
|---|---|
| MPU6050 SDA | GPIO21 |
| MPU6050 SCL | GPIO22 |
| FSR ADC | GPIO32 |
| Plunger Button | GPIO27 |
| Vibration Motor | GPIO26 |
| Buzzer | GPIO25 |
| Green LED | GPIO33 |
| Red LED | GPIO14 |

> Pin assignments for the buzzer and LEDs are provisional until final integration.
