# KOKCHI System Architecture

## 1. Design Principle

KOKCHI는 **Physical Action Sensing → ESP32 State-aware Processing → Real-time Feedback → Session History** 구조를 사용한다.

ESP32가 sensor acquisition, event processing, State Machine, timer, feedback rule을 담당하며 Web HMI는 ESP32의 결과를 표시하고 사용자의 위치/시간 선택 및 session confirmation을 담당한다.

```text
User action
   ↓
┌─────────────────────────────────────┐
│ Inputs                              │
│ FSR-PAD / FSR-PEN / MPU / Plunger │
└─────────────────────────────────────┘
   ↓
┌─────────────────────────────────────┐
│ ESP32 Embedded Core                 │
│ - Sensor acquisition                │
│ - Debounce / event processing       │
│ - State Machine                     │
│ - State-aware orientation           │
│ - Hold timer                        │
│ - Feedback control                  │
└─────────────────────────────────────┘
   ↓                     ↓
LED / Vibration / Buzzer   Web HMI
                           ↓
                     Confirmed History
                           ↓
                       Next Session
```

## 2. Inputs

| Input | Role |
|---|---|
| FSR-PAD | Silicone-pad side Contact Gate |
| FSR-PEN | Pen-side Contact Gate |
| MPU6050 | External pen orientation / hold drift |
| Plunger tact | DOWN/UP event and hold timing |
| Web selection | Site/sub-region and 3/6/10 s hold selection |

## 3. State Machine

Final states:

```text
WAIT_SITE
   │  Web session start
   ▼
READY
   │  PAD + PEN contact confirmed
   ▼
ORIENTATION_CHECK
   │  orientation acceptable + valid Plunger DOWN
   ▼
HOLDING
   ├──────── movement warning ────────┐
   │                                  │
   │                          recovery to stable
   │                                  │
   └──── target time reached ◀────────┘
   │
   ├─ Plunger UP before target → RESULT_INTERRUPTED
   ▼
HOLD_COMPLETE
   │  Plunger release
   ▼
RESULT_SUCCESS
```

### Issue Codes

- `PARTIAL_CONTACT`
- `BAD_ORIENTATION_PRE`
- `PLUNGER_TOO_EARLY`
- `HOLD_MOVEMENT`
- `EARLY_RELEASE`

`HOLD_MOVEMENT`는 warning으로 취급하여 HOLD timer를 유지하며, `EARLY_RELEASE`는 정상완료와 구분하여 `RESULT_INTERRUPTED`로 종료한다.

## 4. State-aware MPU Interpretation

같은 MPU6050 데이터를 모든 단계에서 동일하게 판단하지 않는다.

### Before Plunger DOWN

```text
Current Roll/Pitch
      ↕
TARGET_ROLL / TARGET_PITCH
```

질문: **이 자세로 시작 가능한가?**

### After valid Plunger DOWN

HOLDING 진입 순간:

```cpp
holdStartRoll = rollDeg;
holdStartPitch = pitchDeg;
```

이후에는:

```text
Current Roll/Pitch
      ↕
holdStartRoll / holdStartPitch
```

질문: **시작한 자세에서 수행 중 얼마나 움직였는가?**

따라서 MPU의 reference frame을 수행 State에 따라 전환한다.

## 5. Hold Warning / Recovery

- warning: Roll 또는 Pitch drift > 6.0°가 250 ms 지속
- clear: Roll과 Pitch drift ≤ 4.5°가 200 ms 지속

Warning과 clear 기준을 분리하여 경계 부근의 반복 상태전이를 줄인다.

HOLD movement warning 중:

- Red LED: ON
- Vibration: 사용하지 않음
- Hold timer: 계속 진행
- 안정 범위 복귀 시 Green LED 복원

## 6. Feedback Channels

| Channel | Final role |
|---|---|
| Green LED | 정상 / 진행 가능 / hold 완료 / success |
| Red LED | correction 필요 / warning / interrupted |
| Vibration | pre-injection orientation correction only |
| Buzzer | hold target reached cue |
| Web HMI | 상세 상태, sensor status, result, Rotation history |

## 7. Web Integration

ESP32 provides:

- `GET /api/status`
- `POST /api/session/start`
- `POST /api/session/reset`

Web JavaScript polls `/api/status` and renders the state calculated by the ESP32. Web does not independently fabricate sensor state or session success.

## 8. Rotation Workflow

```text
Region / sub-region selection
        ↓
Recent confirmed history comparison
        ↓
Injection session
        ↓
RESULT_SUCCESS
        ↓
Actual site Confirm / Correct
        ↓
History saved
        ↓
Used in next-session recent comparison
```

Current prototype parameters:

- recent comparison: 3 confirmed sessions
- max browser history: 10 records
- storage: browser `localStorage`

## 9. Prototype Boundary

The architecture verifies execution-support logic only. It does not measure medication dose, needle depth, tissue penetration, or clinical effectiveness.
