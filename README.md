# InjectCoach

ESP32-based real-time self-injection assistance system with sensor-driven feedback.

## Overview

InjectCoach is an embedded system designed to support self-injection procedures by sensing key user actions and providing immediate feedback.

The system integrates physical sensor inputs, embedded decision logic, local feedback devices, and a web-based human-machine interface.

## Core Functions

- Injection site rotation management
- Pen angle monitoring using MPU6050
- Contact detection using FSR-402
- Hold-time measurement using a physical plunger button
- Real-time feedback using LED, vibration, and buzzer
- Web-based status monitoring and session results

## System Architecture

```text
User Action
    ↓
Sensors / User Input
    ↓
ESP32
    ↓
Embedded State Machine
    ↓
Decision Logic
    ↓
Physical Feedback + Web HMI
```

## Hardware

- ESP32 Dev Module
- MPU6050
- FSR-402
- Plunger button
- Vibration motor module
- Active buzzer
- LEDs
- Silicone pad
- Mock injection pen

## Repository Structure

```text
injectcoach-dev/
├── firmware/      # ESP32 firmware
├── web/           # Web interface
├── hardware/      # Wiring and hardware information
├── docs/          # Development logs and test results
├── media/         # Images and documentation assets
└── README.md
```

## Current Development Status

### Completed

- ESP32 environment setup
- Serial communication
- SoftAP Wi-Fi test
- Plunger button input
- Hold-time measurement
- MPU6050 basic sensing
- FSR contact detection
- Vibration motor control

### In Progress

- Sensor integration
- LED integration
- Buzzer GPIO control
- State machine
- Web dashboard
- Rotation history
- End-to-end testing

## Prototype Scope

The current prototype uses a mock injection pen and silicone pad to reproduce the intended interaction flow without using a real needle or medication.

The prototype validates the sensing, decision, and feedback architecture of the system.

It does not evaluate:

- Clinical effectiveness
- Injection depth
- Medication dose
- Medical safety

## Documentation

Detailed development records and test results are available in the [`docs`](./docs) directory.

## Development Status

This repository is currently under active development.
