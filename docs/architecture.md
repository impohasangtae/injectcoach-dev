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
```

## Inputs

- MPU6050
  - Pen tilt / angle

- FSR-402
  - Contact detection

- Plunger Button
  - Injection event and hold time

- Web Interface
  - Injection site selection

## Embedded Processing

ESP32 performs:

- Sensor acquisition
- Contact detection
- Angle evaluation
- Hold-time measurement
- State transitions
- Feedback control
- Wi-Fi communication

## State Machine

```text
IDLE
  ↓
SITE_SELECTED
  ↓
READY
  ↓
CONTACT_DETECTED
  ↓
INJECTING
  ↓
HOLDING
  ↓
RELEASED
  ↓
SCORING
  ↓
RESULT
```

## Outputs

### Local Feedback

- LED
- Vibration
- Buzzer

### Human-Machine Interface

- Injection site
- Rotation history
- Sensor status
- Hold time
- Session result
