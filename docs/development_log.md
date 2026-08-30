# InjectCoach Development Log

## 2026-08-23 — Hardware Bring-up & Sensor Test

### 1. ESP32
- Arduino IDE 환경 구축 완료
- ESP32 Dev Module / COM5 연결 확인
- Serial upload 정상 동작
- SoftAP Wi-Fi 생성 및 스마트폰 연결 성공

### 2. Plunger Button
- GPIO27 사용
- Button press / release 감지 성공
- Hold time 측정 성공

### 3. MPU6050
- SDA: GPIO21
- SCL: GPIO22
- I2C address 0x68 확인
- Accelerometer / Gyroscope 정상 동작
- Roll / Pitch 변화 확인
- 최종 calibration 및 펜 고정은 추후 진행

### 4. FSR-402
- GPIO32 ADC 입력 테스트
- No Contact / Contact 구분 성공
- 강한 압력에서 ADC saturation 확인
- 다단계 압력 분류보다 Contact 판정 중심으로 설계 변경

### 5. Vibration Motor
- GPIO26 제어 성공
- ESP32를 통한 ON/OFF 동작 확인

### 6. Active Buzzer
- 3.3V 직접 전원에서 발음 확인
- GPIO 제어는 transistor/MOSFET 확보 후 진행 예정

### 7. Remaining Tasks
- LED test
- MPU6050 + FSR + Plunger sensor integration
- Contact threshold 결정
- State Machine 구현
- Web dashboard 구현
- Rotation history 구현
- End-to-End test
