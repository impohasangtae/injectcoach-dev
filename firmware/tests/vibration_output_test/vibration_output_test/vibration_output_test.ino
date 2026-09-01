#include <Wire.h>
#include <math.h>

// ===============================
// KOKCHI
// MPU6050 + Vibration Interference Test
// ===============================

// MPU6050
const uint8_t MPU_ADDR = 0x68;
const int SDA_PIN = 21;
const int SCL_PIN = 22;

// Vibration
const int VIBRATION_PIN = 26;

// 이번에 테스트할 진동 길이
const unsigned long VIBRATION_MS = 70;

// 실험 구간
const unsigned long BASELINE_MS = 2000;   // 진동 전 2초
const unsigned long RECOVERY_MS = 1500;   // 진동 후 1.5초

// MPU 출력 주기
const unsigned long SAMPLE_INTERVAL_MS = 20;  // 50 Hz

// 총 반복 횟수
const int TOTAL_TRIALS = 5;


// ===============================
// 실험 상태
// ===============================

enum TestPhase {
  BASELINE,
  VIBRATION,
  RECOVERY,
  FINISHED
};

TestPhase phase = BASELINE;

unsigned long phaseStart = 0;
unsigned long lastSample = 0;

int trial = 1;


// ===============================
// MPU6050 초기화
// ===============================

bool initMPU() {

  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);      // PWR_MGMT_1
  Wire.write(0x00);      // Wake up
  if (Wire.endTransmission() != 0) {
    return false;
  }

  delay(100);

  // WHO_AM_I 확인
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x75);
  Wire.endTransmission(false);

  Wire.requestFrom(MPU_ADDR, (uint8_t)1);

  if (Wire.available()) {
    uint8_t whoAmI = Wire.read();

    Serial.print("# WHO_AM_I = 0x");
    Serial.println(whoAmI, HEX);

    if (whoAmI == 0x68 || whoAmI == 0x69) {
      return true;
    }
  }

  return false;
}


// ===============================
// MPU6050 Roll / Pitch 읽기
// ===============================

bool readMPU(float &roll, float &pitch) {

  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);  // ACCEL_XOUT_H

  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  Wire.requestFrom(MPU_ADDR, (uint8_t)6);

  if (Wire.available() < 6) {
    return false;
  }

  int16_t rawAx = (Wire.read() << 8) | Wire.read();
  int16_t rawAy = (Wire.read() << 8) | Wire.read();
  int16_t rawAz = (Wire.read() << 8) | Wire.read();

  float ax = rawAx / 16384.0;
  float ay = rawAy / 16384.0;
  float az = rawAz / 16384.0;

  roll =
      atan2(ay, az) * 180.0 / PI;

  pitch =
      atan2(-ax, sqrt(ay * ay + az * az))
      * 180.0 / PI;

  return true;
}


// ===============================
// Phase 이름
// ===============================

const char* phaseName(TestPhase p) {

  switch (p) {
    case BASELINE:
      return "BASELINE";

    case VIBRATION:
      return "VIBRATION";

    case RECOVERY:
      return "RECOVERY";

    case FINISHED:
      return "FINISHED";

    default:
      return "UNKNOWN";
  }
}


// ===============================
// SETUP
// ===============================

void setup() {

  Serial.begin(115200);
  delay(500);

  // Vibration
  pinMode(VIBRATION_PIN, OUTPUT);
  digitalWrite(VIBRATION_PIN, LOW);

  // MPU6050
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);

  Serial.println();
  Serial.println("================================");
  Serial.println("KOKCHI MPU + Vibration Test");
  Serial.println("================================");

  if (!initMPU()) {

    Serial.println("ERROR: MPU6050 NOT FOUND");
    Serial.println("Check VCC / GND / SDA21 / SCL22");

    while (true) {
      digitalWrite(VIBRATION_PIN, LOW);
      delay(1000);
    }
  }

  Serial.println("MPU6050 = OK");
  Serial.println("Vibration pulse = 70 ms");
  Serial.println();

  Serial.println("trial,time_ms,phase,roll,pitch");

  phase = BASELINE;
  phaseStart = millis();
}


// ===============================
// LOOP
// ===============================

void loop() {

  unsigned long now = millis();


  // -------------------------------
  // Phase 전환
  // -------------------------------

  if (phase == BASELINE) {

    digitalWrite(VIBRATION_PIN, LOW);

    if (now - phaseStart >= BASELINE_MS) {

      phase = VIBRATION;
      phaseStart = now;

      digitalWrite(VIBRATION_PIN, HIGH);

      Serial.println("# >>> VIBRATION ON");
    }
  }


  else if (phase == VIBRATION) {

    if (now - phaseStart >= VIBRATION_MS) {

      digitalWrite(VIBRATION_PIN, LOW);

      phase = RECOVERY;
      phaseStart = now;

      Serial.println("# <<< VIBRATION OFF");
    }
  }


  else if (phase == RECOVERY) {

    digitalWrite(VIBRATION_PIN, LOW);

    if (now - phaseStart >= RECOVERY_MS) {

      trial++;

      if (trial > TOTAL_TRIALS) {

        phase = FINISHED;

        digitalWrite(VIBRATION_PIN, LOW);

        Serial.println();
        Serial.println("# ==============================");
        Serial.println("# TEST FINISHED");
        Serial.println("# ==============================");
      }

      else {

        phase = BASELINE;
        phaseStart = now;

        Serial.println();
        Serial.print("# ----- TRIAL ");
        Serial.print(trial);
        Serial.println(" -----");
      }
    }
  }


  // -------------------------------
  // MPU Sampling
  // -------------------------------

  if (phase != FINISHED &&
      now - lastSample >= SAMPLE_INTERVAL_MS) {

    lastSample = now;

    float roll;
    float pitch;

    bool ok = readMPU(roll, pitch);

    Serial.print(trial);
    Serial.print(",");
    Serial.print(now);
    Serial.print(",");
    Serial.print(phaseName(phase));
    Serial.print(",");

    if (ok) {

      Serial.print(roll, 2);
      Serial.print(",");
      Serial.println(pitch, 2);

    } else {

      Serial.println("ERROR,ERROR");
    }
  }
}