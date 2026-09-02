# KOKCHI Test Results

본 문서는 현재 개발완료보고서에서 사용할 수 있는 **실제 측정·검증 근거**만 정리한다. 의료 성능시험이 아니라 기능 프로토타입의 engineering validation 기록이다.

## 1. ESP32 / Communication

| Test | Result |
|---|---|
| Serial upload / monitor | PASS |
| ESP32 SoftAP | PASS |
| Smartphone connection | PASS |
| WebServer / HMI connection | PASS |
| ESP32 status API integration | PASS |

## 2. FSR-PAD Calibration

### No Contact, 10 trials

```text
0, 0, 0, 0, 0, 0, 0, 0, 0, 0
```

### Recorded Contact trials

```text
3929, 3952, 4093, 4095, 4087,
2682, 2459, 4090, 3621, 4090
```

`2682`, `2459`는 접촉 적용과 측정 timing이 맞지 않았던 procedural invalid trial로 개발기록에 분리하였다.

Valid Contact range:

```text
3621–4095
```

Final threshold:

```cpp
FSR_PAD_THRESHOLD = 2000;
```

**Engineering decision:** multi-level pressure score가 아니라 **Contact / No Contact Gate**로 사용한다.

## 3. FSR-PEN Calibration

### No Contact, 10 trials

```text
0, 0, 0, 0, 0, 0, 0, 0, 0, 0
```

### Contact, 10 trials

```text
3714, 3645, 3556, 3534, 3545,
3184, 3193, 3608, 3622, 3724
```

Contact range:

```text
3184–3724
```

Final threshold:

```cpp
FSR_PEN_THRESHOLD = 1500;
```

**Engineering decision:** FSR-PEN 역시 정밀 힘(N) 측정이 아니라 Contact Gate로 사용한다.

## 4. MPU6050 Final Reference Pose

최종 Grip / UP 방향을 고정한 상태의 3회 측정:

| Trial | Roll | Pitch |
|---:|---:|---:|
| 1 | -7.33° | 52.49° |
| 2 | -8.10° | 53.28° |
| 3 | -7.25° | 52.71° |

Mean:

```cpp
TARGET_ROLL  = -7.56f;
TARGET_PITCH = 52.83f;
```

이 값은 현재 prototype의 Demo Profile reference이며 보편적인 의료 각도 기준이 아니다.

## 5. Vibration–IMU Interference

실험 흐름:

```text
baseline 2 s
→ vibration 70 ms
→ recovery observation 1.5 s
```

관찰:

- 70 ms vibration 중 accelerometer 기반 Roll/Pitch가 순간적으로 교란됨
- 진동 종료 후 약 **180–200 ms**에서 baseline 근처로 회복

Final engineering parameters:

```cpp
VIBRATION_MS = 70;
IMU_SETTLE_MS = 250;
```

Firmware는 vibration 시작 후 `70 ms + 250 ms` 구간 동안 IMU 값을 orientation GOOD/BAD 판단에 사용하지 않는다.

## 6. State Machine Parameters in Final Integrated Firmware

```text
Pre orientation tolerance     ±7.0° (Roll/Pitch)
Pre bad confirm              350 ms
Hold drift warning           > 6.0° for 250 ms
Hold drift clear             ≤ 4.5° for 200 ms
Buzzer                       350 ms
Plunger debounce             30 ms
Hold targets                 3 / 6 / 10 s
```

위 값은 final integrated firmware에 포함된 Demo Profile engineering parameter다.

## 7. Functional State Validation

확인된 핵심 흐름:

- Contact Gate → `READY` / `ORIENTATION_CHECK`
- pre-orientation error → Red + vibration
- orientation recovery → Green
- valid Plunger DOWN → HOLD reference capture + timer start
- HOLD movement → Red warning, timer continues
- HOLD stability recovery → Green
- target hold reached → buzzer + `HOLD_COMPLETE`
- target before Plunger release → `EARLY_RELEASE` → `RESULT_INTERRUPTED`
- hold complete after release → `RESULT_SUCCESS`

## 8. Rotation / Web Validation

Final Web behavior:

- site / sub-region selection
- recent confirmed history comparison
- session start / status polling / reset
- success 후 실제 위치 Confirm/Correct
- Confirm 이후에만 history 저장
- interrupted session은 정상 완료 history로 저장하지 않음
- `localStorage` persistence across browser reloads

Parameters:

```text
recentWindow = 3
maxHistory   = 10
storage key  = kokchi_history_v1
```

## 9. Important Interpretation Boundary

이 문서의 PASS와 수치는 다음을 의미한다.

- 기능 구현 여부
- 센서 입력의 prototype-level 분리 가능성
- State Machine / physical feedback / Web integration 동작

다음을 의미하지 않는다.

- 임상적 정확도
- 치료효과 향상
- 합병증 예방
- 실제 약물 투여 안전성
