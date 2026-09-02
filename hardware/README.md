# KOKCHI Hardware

## Final Components

- ESP32 Dev Module
- MPU6050
- FSR-402 × 2
- Plunger tact switch
- Vibration motor module
- Active buzzer
- 2N2222A transistor + 1 kΩ base resistor
- Green LED / Red LED
- Silicone pad
- Mock injection pen

## Final Pin Map

| Function | ESP32 GPIO | Role |
|---|---:|---|
| MPU6050 SDA | 21 | I2C data |
| MPU6050 SCL | 22 | I2C clock |
| FSR-PAD | 32 | Pad-side Contact Gate |
| FSR-PEN | 34 | Pen-side Contact Gate |
| Plunger tact | 27 | DOWN / UP event |
| Vibration | 26 | Pre-orientation tactile correction |
| Active buzzer driver | 25 | Hold completion cue |
| Green LED | 33 | Normal / proceed / complete |
| Red LED | 14 | Correction / warning / interrupted |

## Buzzer Driver

```text
ESP32 GPIO25
   ↓
1 kΩ base resistor
   ↓
2N2222A transistor
   ↓
Active buzzer
```

## FSR Role

FSR-PAD와 FSR-PEN은 정밀 force(N) 계측이 아니라 Contact / No Contact Gate로 사용한다.

```text
FSR_PAD_THRESHOLD = 2000
FSR_PEN_THRESHOLD = 1500
```

측정 근거는 [`../docs/test_results.md`](../docs/test_results.md)를 참고한다.

## MPU Mounting / Reference

최종 사용자는 mock pen에 표시한 Grip / UP 방향을 유지한다. Final reference:

```text
TARGET_ROLL  = -7.56°
TARGET_PITCH = 52.83°
```

이 값은 현재 physical prototype / Demo Profile에 대한 engineering reference이다.
