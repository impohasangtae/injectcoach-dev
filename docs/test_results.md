# InjectCoach Test Results

This document records quantitative and repeatability tests for the InjectCoach prototype.

## 1. ESP32

### Serial Communication
Status: PASS

### SoftAP Wi-Fi
Status: PASS

---

## 2. Plunger Button

### Press / Release Detection
Status: PASS

### Hold-Time Measurement
Status: PASS

Detailed repeated measurements: To be added.

---

## 3. MPU6050

### I2C Detection
Address: `0x68`

### Accelerometer / Gyroscope
Status: PASS

### Roll / Pitch
Status: PASS

### Repeatability Test
To be added after rigid mounting and calibration.

---

## 4. FSR-402

### Contact Detection
Status: PASS

Observed ADC range:
- No contact: approximately 0
- Contact: increases substantially
- Strong input may saturate near 4095

### Pressure Classification
Multi-level pressure classification is not currently used because overlapping ranges and ADC saturation were observed.

Final contact threshold: To be determined.

---

## 5. Vibration Motor

Status: PASS

Influence on MPU6050 measurements: To be tested.

---

## 6. Buzzer

Direct 3.3 V operation: PASS

GPIO-controlled operation: Pending driver circuit.

---

## 7. End-to-End Test

Status: Pending
