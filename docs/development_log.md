# KOKCHI Development Log

본 문서는 Git history와 주요 실험기록을 기준으로 개발의 실제 흐름을 요약한다.

## 2026-08-30 — Repository & Base Structure

- Git repository 및 기본 directory 구조 생성
- ESP32 / sensor / Web / hardware / docs 구조 정리
- 초기 hardware test와 architecture 문서화

## 2026-08-31 — Input Integration & Web/Rotation

Git milestones:

- `feat: add dual FSR and plunger input firmware`
- `feat: add ESP32 web dashboard prototype`
- `feat: add rotation assistance workflow prototype`
- `test: verify MPU6050 after soldering`
- `feat: integrate rotation and injection web workflow`

주요 개발:

- Dual FSR(PAD/PEN) 독립 입력
- Plunger tact input
- ESP32 SoftAP / Web dashboard
- 위치 선택과 Rotation history workflow
- MPU6050 단계별 재검증

## 2026-09-01 — UX Refinement & Full I/O

Git milestones:

- `feat: finalize web navigation workflow`
- `feat: refine rotation profile and history UX`
- `test: add hardware integration and output validation`

주요 개발:

- Web navigation / Rotation confirm 흐름 정리
- LED / vibration / buzzer 포함 Full I/O 통합 확인
- 물리 시제품 wiring / sensor mounting 통합

## 2026-09-02 — Calibration, State Machine, Final Integration

Git milestones:

- `feat: add sensor-driven state machine prototype`
- `test: add sensor calibration and validation tests`
- `test: verify final state machine feedback flow`
- `refactor: prepare state machine core for web integration`
- `link esp and website v1`
- `Persist rotation history across browser sessions`

주요 개발:

### Sensor calibration

- FSR-PAD / PEN Contact Gate threshold 확정
- 최종 Grip / UP 기준 MPU reference 3회 측정

### Engineering Decision 1 — Pressure → Contact Gate

초기에는 FSR로 압력 단계를 구분하려 했으나, 강한 입력에서 ADC saturation과 단계간 overlap을 관찰하였다.

따라서 최종적으로 센서가 안정적으로 구분하는 **Contact / No Contact** 정보만 사용하도록 역할을 제한하였다.

### Engineering Decision 2 — Vibration–IMU interference

pre-orientation correction용 vibration이 MPU 자세값 자체를 순간적으로 교란하는 것을 확인하였다.

- vibration: 70 ms
- observed recovery: 약 180–200 ms
- final decision exclusion: 250 ms

출력 피드백이 다시 sensor error를 유발하는 loop를 줄이기 위해 sensing decision timing을 분리하였다.

### Engineering Decision 3 — State-aware MPU interpretation

처음에는 모든 단계에서 하나의 절대 target과 비교하려 했으나, 수행 전과 HOLD 중의 판단 목적이 다르다고 판단하였다.

- 수행 전: 고정 target과 절대 비교
- valid Plunger DOWN 이후: 그 순간 pose를 저장하고 상대 drift 비교

### Engineering Decision 4 — Plunger level → event-aware processing

단순 LOW/HIGH만으로는 새 DOWN event와 이미 눌린 상태를 구분하기 어려워 `PLUNGER_TOO_EARLY` 오판 가능성이 있었다.

최종 firmware는 debounce된 DOWN/UP event와 `pendingPlungerStart`를 사용해 State transition을 처리한다.

### Final Web / History

- ESP32 status API + Web HMI 연결
- Hold 3 / 6 / 10 s 선택
- 성공 후 실제 위치 Confirm/Correct
- Confirm된 history만 browser `localStorage`에 저장

## Current Final Source

```text
firmware/kokchi_esp32/kokchi_esp32.ino
```

이 파일이 현재 보고서·영상·GitHub에서 기준으로 삼는 final integrated source이다.
