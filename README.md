# KOKCHI (콕치)

자가주사 수행 순간의 물리 행동을 감지하고, ESP32가 현재 수행 단계(State)에 따라 해석하여 즉시 피드백하는 임베디드 수행 보조 기능 프로토타입입니다.

> 본 저장소의 이전 개발명은 **InjectCoach**였습니다. 현재 작품명은 **KOKCHI(콕치)** 입니다.

## 핵심 구조

```text
사용자 행동
  ↓
FSR-PAD / FSR-PEN / MPU6050 / Plunger
  ↓
ESP32
  ├─ Sensor acquisition
  ├─ Event processing
  ├─ State machine
  ├─ Rule evaluation
  └─ Hold timer
  ↓
LED / Vibration / Buzzer + Web HMI
  ↓
Session result → Confirmed Rotation History → 다음 세션
```

ESP32가 센서 판정과 상태 전이의 **single source of truth**이며, Web은 상태 표시·위치 선택·결과 확인·Rotation history를 담당하는 HMI입니다.

## 구현된 핵심 기능

- Dual FSR 기반 Contact Gate
  - FSR-PAD: GPIO32, threshold 2000
  - FSR-PEN: GPIO34, threshold 1500
- MPU6050 기반 State-aware orientation
  - 수행 전: 고정 Reference Pose와 절대 비교
  - Plunger DOWN 이후: 시작 자세를 저장하고 상대 drift 비교
- Plunger DOWN/UP event 및 Hold timer
- Hold 중 movement warning / recovery / early release interruption 분리
- 상태별 Green/Red LED, pre-orientation vibration, hold-completion buzzer
- ESP32 SoftAP + WebServer 기반 모바일 HMI
- 위치 선택 → 최근 이력 비교 → 실제 세션 → 성공 후 위치 Confirm → Rotation history 저장
- Hold time 선택: 3 / 6 / 10초 (Demo Profile parameter)

## 최종 State Machine

```text
WAIT_SITE
   ↓
READY
   ↓
ORIENTATION_CHECK
   ↓
HOLDING
   ├─ movement warning → recovery 가능, timer 지속
   └─ early plunger release → RESULT_INTERRUPTED
   ↓
HOLD_COMPLETE
   ↓
RESULT_SUCCESS
```

세부 상태·Issue 정의는 [`docs/architecture.md`](./docs/architecture.md)를 참고하세요.

## Hardware

- ESP32 Dev Module
- MPU6050
- FSR-402 × 2
- Plunger tact switch
- Vibration motor module
- Active buzzer + 2N2222A driver
- Green / Red LED
- Silicone pad
- Mock injection pen

최종 pin map은 [`hardware/README.md`](./hardware/README.md)에 정리되어 있습니다.

## Repository Structure

```text
injectcoach-dev/
├─ firmware/
│  ├─ kokchi_esp32/             # 최종 통합 firmware
│  ├─ kokchi_state_machine/     # Web 통합 전 State Machine 검증본
│  └─ tests/                    # 센서·출력·통합 검증 sketch
├─ hardware/                    # 회로/핀맵/하드웨어 설명
├─ web/                         # Web HMI 구조 설명
├─ docs/                        # architecture, 개발기록, 정량 결과
├─ media/                       # 시연/스크린샷 증거자료
├─ LICENSE_NOTES.md             # 외부 framework/library 안내
└─ README.md
```

## Final Firmware

최종 통합본:

```text
firmware/kokchi_esp32/kokchi_esp32.ino
```

Web HMI도 별도 서버 파일이 아니라 위 firmware 내부의 HTML/CSS/JavaScript로 포함되어 있습니다.

### Web API

- `GET /api/status`
- `POST /api/session/start`
- `POST /api/session/reset`

## Rotation History

현재 프로토타입에서는 성공 세션 후 사용자가 실제 수행 위치를 Confirm한 경우에만 history에 추가합니다.

- 저장: browser `localStorage`
- key: `kokchi_history_v1`
- recent comparison window: 최근 3회
- max history: 10회

동일 브라우저에서는 재접속 후에도 유지되지만, 다른 기기와 자동 동기화되지는 않습니다.

## Prototype Scope

현재 시스템은 모형 주사기와 실리콘 패드를 사용하여 **센싱 → 상태 판단 → 물리 피드백 → Web/History 연결**의 기능적 구조를 검증한 프로토타입입니다.

현재 측정하거나 검증하지 않는 항목:

- 실제 약물 투여량
- 바늘 삽입 깊이
- 피하조직 도달 여부
- 임상적 안전성·유효성
- 의료적 합병증 감소 효과

각 threshold와 hold 설정은 현재 Demo Profile을 위한 engineering parameter이며, 보편적인 의료 기준으로 사용하지 않습니다.

## Test Evidence

정량값과 주요 Engineering Decision은 [`docs/test_results.md`](./docs/test_results.md)에 정리되어 있습니다.

## Development History

2026-08-30부터 2026-09-02까지의 주요 Git milestone은 [`docs/development_log.md`](./docs/development_log.md)에 정리되어 있습니다.

## Third-party Software

본 firmware는 Arduino-ESP32 개발환경의 `WiFi`, `WebServer`, `Wire` 라이브러리를 사용합니다. 저장소에는 해당 library source를 복사하거나 수정하여 포함하지 않았습니다. 자세한 내용은 [`LICENSE_NOTES.md`](./LICENSE_NOTES.md)를 참고하세요.
