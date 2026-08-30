# InjectCoach System Architecture

## System Concept

InjectCoach follows a sensor-to-decision-to-feedback architecture.

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
