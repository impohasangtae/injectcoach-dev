# Firmware

KOKCHI ESP32 firmware와 단계별 검증 sketch를 보관한다.

## Final Integrated Firmware

```text
kokchi_esp32/kokchi_esp32.ino
```

Final firmware includes:

- MPU6050 I2C read / Roll-Pitch calculation
- Dual FSR Contact Gate
- Plunger debounce + DOWN/UP events
- State Machine
- State-aware pre/hold orientation logic
- LED / vibration / buzzer control
- Hold timer: 3 / 6 / 10 s
- ESP32 SoftAP / WebServer
- `/api/status`, `/api/session/start`, `/api/session/reset`
- Embedded Web HMI
- Rotation workflow / browser history

## Standalone State Machine

```text
kokchi_state_machine/kokchi_state_machine.ino
```

Web integration 전에 sensor-driven State Machine과 physical feedback flow를 독립 검증하기 위해 사용한 development checkpoint다. 최종 제출 기준 source는 `kokchi_esp32.ino`이다.

## Test Sketches

```text
tests/
├─ fsr_pen_test/
├─ fsr_threshold_calibration/
├─ kokchi_full_io_integration_test/
├─ kokchi_input_integration_test/
├─ kokchi_state_machine_test/
├─ led_output_test/
├─ mpu6050_post_solder_test/
├─ mpu_error_pose_test/
├─ mpu_grip_final_validation/
├─ mpu_reference_calibration/
├─ vibration_mpu_interference_test/
└─ vibration_output_test/
```

`buzzer_output_test`는 별도 단독 test가 아니라 Full I/O integration code와 중복되어 final repository에서는 제거하였다. Buzzer 동작은 Full I/O 및 final integrated firmware에서 확인한다.

## Final Parameter Source

최종 값은 반드시 `kokchi_esp32/kokchi_esp32.ino`를 기준으로 한다. 초기/중간 test sketch의 candidate parameter는 final setting과 다를 수 있다.
