#include <Wire.h>
#include <math.h>

// =========================
// PIN
// =========================
const int FSR_PAD_PIN = 32;
const int FSR_PEN_PIN = 34;
const int PLUNGER_PIN  = 27;

const int SDA_PIN = 21;
const int SCL_PIN = 22;

const uint8_t MPU_ADDR = 0x68;

// =========================
// MPU
// =========================
bool mpuOK = false;

// =========================
// PLUNGER / HOLD
// =========================
bool lastRawPressed = false;
bool stablePressed = false;

unsigned long debounceStart = 0;
const unsigned long DEBOUNCE_MS = 25;

unsigned long holdStart = 0;
float lastHoldSec = 0.0;

// =========================
// SERIAL
// =========================
unsigned long lastPrint = 0;
const unsigned long PRINT_INTERVAL_MS = 200;


void setup() {
  Serial.begin(115200);
  delay(500);

  // Plunger
  pinMode(PLUNGER_PIN, INPUT_PULLUP);

  // ADC
  analogReadResolution(12);

  // MPU6050
  Wire.begin(SDA_PIN, SCL_PIN);

  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);       // PWR_MGMT_1
  Wire.write(0x00);       // Wake up
  mpuOK = (Wire.endTransmission() == 0);

  Serial.println();
  Serial.println("======================================");
  Serial.println("KOKCHI INPUT INTEGRATION TEST");
  Serial.println("MPU + FSR-PAD + FSR-PEN + PLUNGER");
  Serial.println("======================================");

  if (mpuOK) {
    Serial.println("MPU INIT = OK");
  } else {
    Serial.println("MPU INIT = ERROR");
  }
}


bool readMPU(float &roll, float &pitch) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);

  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  uint8_t count = Wire.requestFrom(
    MPU_ADDR,
    (uint8_t)14,
    (uint8_t)true
  );

  if (count != 14) {
    return false;
  }

  int16_t ax = (Wire.read() << 8) | Wire.read();
  int16_t ay = (Wire.read() << 8) | Wire.read();
  int16_t az = (Wire.read() << 8) | Wire.read();

  // Temperature: 현재 사용하지 않음
  Wire.read();
  Wire.read();

  // Gyro: 데이터는 읽되 현재 출력에는 사용하지 않음
  int16_t gx = (Wire.read() << 8) | Wire.read();
  int16_t gy = (Wire.read() << 8) | Wire.read();
  int16_t gz = (Wire.read() << 8) | Wire.read();

  roll = atan2((float)ay, (float)az) * 180.0 / PI;

  pitch = atan2(
    -(float)ax,
    sqrt((float)ay * ay + (float)az * az)
  ) * 180.0 / PI;

  return true;
}


void updatePlunger() {
  unsigned long now = millis();

  // INPUT_PULLUP:
  // 안 누름 = HIGH
  // 누름   = LOW
  bool rawPressed = (digitalRead(PLUNGER_PIN) == LOW);

  // 상태 변화 감지 → debounce 시작
  if (rawPressed != lastRawPressed) {
    debounceStart = now;
    lastRawPressed = rawPressed;
  }

  // 일정 시간 같은 상태가 유지됐으면 실제 상태로 인정
  if ((now - debounceStart >= DEBOUNCE_MS) &&
      (rawPressed != stablePressed)) {

    stablePressed = rawPressed;

    if (stablePressed) {
      holdStart = now;

      Serial.println();
      Serial.println(">>> PLUNGER DOWN");
    }
    else {
      lastHoldSec = (now - holdStart) / 1000.0;

      Serial.println();
      Serial.print(">>> PLUNGER RELEASE / HOLD = ");
      Serial.print(lastHoldSec, 2);
      Serial.println(" sec");
    }
  }
}


void loop() {
  updatePlunger();

  unsigned long now = millis();

  if (now - lastPrint >= PRINT_INTERVAL_MS) {
    lastPrint = now;

    // -------------------------
    // FSR
    // -------------------------
    int fsrPad = analogRead(FSR_PAD_PIN);
    int fsrPen = analogRead(FSR_PEN_PIN);

    // -------------------------
    // MPU
    // -------------------------
    float roll = 0.0;
    float pitch = 0.0;

    bool mpuReadOK = false;

    if (mpuOK) {
      mpuReadOK = readMPU(roll, pitch);
    }

    // -------------------------
    // HOLD
    // -------------------------
    float holdSec;

    if (stablePressed) {
      holdSec = (now - holdStart) / 1000.0;
    } else {
      holdSec = lastHoldSec;
    }

    // -------------------------
    // OUTPUT
    // -------------------------
    Serial.print("MPU=");
    Serial.print(mpuReadOK ? "OK" : "ERR");

    Serial.print(" | ROLL=");
    Serial.print(roll, 1);

    Serial.print(" | PITCH=");
    Serial.print(pitch, 1);

    Serial.print(" | FSR_PAD=");
    Serial.print(fsrPad);

    Serial.print(" | FSR_PEN=");
    Serial.print(fsrPen);

    Serial.print(" | PLUNGER=");
    Serial.print(stablePressed ? "DOWN" : "UP");

    Serial.print(" | HOLD=");
    Serial.print(holdSec, 2);

    Serial.println("s");
  }

  delay(5);
}