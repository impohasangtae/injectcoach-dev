# KOKCHI Web HMI

KOKCHI Web은 별도의 cloud/server application이 아니라 ESP32가 직접 제공하는 local HMI다.

## Role

Web HMI responsibilities:

- injection region / sub-region selection
- hold target selection: 3 / 6 / 10 s
- current ESP32 state / issue display
- contact / orientation / plunger / hold progress display
- session result
- post-success actual-site Confirm / Correct
- Rotation history display

Sensor acquisition과 최종 State 판단은 ESP32 firmware가 수행한다.

## API

```text
GET  /api/status
POST /api/session/start
POST /api/session/reset
```

JavaScript는 `/api/status`를 polling하여 ESP32가 계산한 state를 화면에 반영한다.

## Rotation History

```text
storage       = browser localStorage
key           = kokchi_history_v1
recentWindow  = 3
maxHistory    = 10
```

History는 성공한 session 이후 실제 수행 위치를 사용자가 Confirm한 경우에만 저장한다.

현재 prototype에서는 동일 browser에서 재접속 시 history가 유지되지만, 다른 device와 동기화하지 않는다.

## Source Location

HTML/CSS/JavaScript는 final firmware 안의 `PAGE` string으로 포함되어 있다.

```text
../firmware/kokchi_esp32/kokchi_esp32.ino
```

현재 저장소에는 외부 CDN, 외부 JavaScript framework, Google Fonts dependency가 없다.
