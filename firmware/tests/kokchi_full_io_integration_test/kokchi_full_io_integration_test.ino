#include <Wire.h>
#include <math.h>

// =====================================================
// KOKCHI - Full I/O Integration Test
//
// INPUT
// - IMU SDA : GPIO21
// - IMU SCL : GPIO22
// - FSR-PAD : GPIO32
// - FSR-PEN : GPIO34
// - Plunger : GPIO27
//
// OUTPUT
// - Vibration : GPIO26
// - Buzzer    : GPIO25
// - Green LED : GPIO33
// - Red LED   : GPIO14
//
// IMPORTANT
// - Threshold / State Machine 아직 적용하지 않음
// - Raw sensor + 모든 output 동시 동작 확인용
// =====================================================


// =====================================================
// PIN MAP
// =====================================================

const int SDA_PIN = 21;
const int SCL_PIN = 22;

const int FSR_PAD_PIN = 32;
const int FSR_PEN_PIN = 34;
const int PLUNGER_PIN = 27;

const int VIBRATION_PIN = 26;
const int BUZZER_PIN = 25;
const int GREEN_LED_PIN = 33;
const int RED_LED_PIN = 14;


// =====================================================
// IMU
// =====================================================

const uint8_t IMU_ADDR = 0x68;


// =====================================================
// OUTPUT PARAMETERS
// =====================================================

// 오늘 실험으로 잡은 값
const unsigned long VIBRATION_MS = 70;

// 진동 종료 후 IMU 판정 보호시간 후보
const unsigned long IMU_SETTLE_MS = 250;

// 부저 완료음 테스트 길이
const unsigned long BUZZER_MS = 100;


// =====================================================
// BUTTON / SERIAL
// =====================================================

const unsigned long DEBOUNCE_MS = 30;
const unsigned long PRINT_INTERVAL_MS = 200;


// =====================================================
// GLOBAL SENSOR VALUES
// =====================================================

float rollDeg = 0.0;
float pitchDeg = 0.0;

bool imuReadOK = false;

int fsrPadValue = 0;
int fsrPenValue = 0;


// =====================================================
// PLUNGER VARIABLES
// =====================================================

bool lastRawPlunger = HIGH;
bool stablePlunger = HIGH;

unsigned long debounceStart = 0;
unsigned long plungerPressStart = 0;

float lastHoldSec = 0.0;


// =====================================================
// OUTPUT TIMERS
// =====================================================

bool vibrationActive = false;
bool buzzerActive = false;

unsigned long vibrationEndTime = 0;
unsigned long buzzerEndTime = 0;

unsigned long imuIgnoreUntil = 0;


// =====================================================
// GENERAL TIMERS
// =====================================================

unsigned long lastPrintTime = 0;


// =====================================================
// IMU INIT
// =====================================================

bool initIMU() {

  // Wake up
  Wire.beginTransmission(IMU_ADDR);
  Wire.write(0x6B);
  Wire.write(0x00);

  if (Wire.endTransmission() != 0) {
    return false;
  }

  delay(100);


  // WHO_AM_I
  Wire.beginTransmission(IMU_ADDR);
  Wire.write(0x75);

  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  Wire.requestFrom(IMU_ADDR, (uint8_t)1);

  if (!Wire.available()) {
    return false;
  }

  uint8_t whoAmI = Wire.read();

  Serial.print("WHO_AM_I = 0x");
  if (whoAmI < 0x10) Serial.print("0");
  Serial.println(whoAmI, HEX);


  // 이번 보드에서는 0x70 확인됨.
  // 모델명을 강제로 단정하지 않고 통신 가능 여부 중심으로 판단.
  if (whoAmI == 0x00 || whoAmI == 0xFF) {
    return false;
  }

  return true;
}


// =====================================================
// READ IMU
// =====================================================

bool readIMU(float &roll, float &pitch) {

  Wire.beginTransmission(IMU_ADDR);
  Wire.write(0x3B);

  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  Wire.requestFrom(IMU_ADDR, (uint8_t)6);

  if (Wire.available() < 6) {
    return false;
  }


  int16_t rawAx =
      ((int16_t)Wire.read() << 8) | Wire.read();

  int16_t rawAy =
      ((int16_t)Wire.read() << 8) | Wire.read();

  int16_t rawAz =
      ((int16_t)Wire.read() << 8) | Wire.read();


  float ax = rawAx / 16384.0;
  float ay = rawAy / 16384.0;
  float az = rawAz / 16384.0;


  roll =
      atan2(ay, az) *
      180.0 / PI;


  pitch =
      atan2(
        -ax,
        sqrt(ay * ay + az * az)
      ) *
      180.0 / PI;


  return true;
}


// =====================================================
// OUTPUT FUNCTIONS
// =====================================================

void allOutputsOff() {

  digitalWrite(GREEN_LED_PIN, LOW);
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(VIBRATION_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  vibrationActive = false;
  buzzerActive = false;
}


void greenOn() {

  digitalWrite(GREEN_LED_PIN, HIGH);
  digitalWrite(RED_LED_PIN, LOW);

  Serial.println(">>> GREEN LED ON");
}


void redOn() {

  digitalWrite(GREEN_LED_PIN, LOW);
  digitalWrite(RED_LED_PIN, HIGH);

  Serial.println(">>> RED LED ON");
}


void startVibration() {

  unsigned long now = millis();

  digitalWrite(VIBRATION_PIN, HIGH);

  vibrationActive = true;
  vibrationEndTime = now + VIBRATION_MS;

  // 진동시간 + 안정화시간 동안
  // IMU를 읽더라도 판정에 쓰지 않을 예정
  imuIgnoreUntil =
      now + VIBRATION_MS + IMU_SETTLE_MS;

  Serial.println(">>> VIBRATION 70 ms");
}


void startBuzzer() {

  unsigned long now = millis();

  digitalWrite(BUZZER_PIN, HIGH);

  buzzerActive = true;
  buzzerEndTime = now + BUZZER_MS;

  Serial.println(">>> BUZZER 100 ms");
}


// =====================================================
// NON-BLOCKING OUTPUT UPDATE
// =====================================================

void updateOutputs() {

  unsigned long now = millis();


  if (vibrationActive &&
      (long)(now - vibrationEndTime) >= 0) {

    digitalWrite(VIBRATION_PIN, LOW);
    vibrationActive = false;

    Serial.println("<<< VIBRATION OFF");
  }


  if (buzzerActive &&
      (long)(now - buzzerEndTime) >= 0) {

    digitalWrite(BUZZER_PIN, LOW);
    buzzerActive = false;

    Serial.println("<<< BUZZER OFF");
  }
}


// =====================================================
// PLUNGER UPDATE
// =====================================================

void updatePlunger() {

  unsigned long now = millis();

  bool raw = digitalRead(PLUNGER_PIN);


  // raw state changed
  if (raw != lastRawPlunger) {

    lastRawPlunger = raw;
    debounceStart = now;
  }


  // stable long enough
  if ((now - debounceStart >= DEBOUNCE_MS) &&
      raw != stablePlunger) {

    stablePlunger = raw;


    // DOWN
    if (stablePlunger == LOW) {

      plungerPressStart = now;

      Serial.println(">>> PLUNGER DOWN");
    }


    // RELEASE
    else {

      lastHoldSec =
          (now - plungerPressStart) / 1000.0;

      Serial.print("<<< PLUNGER RELEASE | HOLD=");
      Serial.print(lastHoldSec, 2);
      Serial.println("s");
    }
  }
}


// =====================================================
// CURRENT HOLD
// =====================================================

float getCurrentHoldSec() {

  if (stablePlunger == LOW) {

    return
      (millis() - plungerPressStart)
      / 1000.0;
  }

  return lastHoldSec;
}


// =====================================================
// SERIAL COMMANDS
// =====================================================

void handleSerialCommands() {

  while (Serial.available()) {

    char c = Serial.read();


    if (c == 'g' || c == 'G') {

      greenOn();
    }


    else if (c == 'r' || c == 'R') {

      redOn();
    }


    else if (c == 'v' || c == 'V') {

      startVibration();
    }


    else if (c == 'b' || c == 'B') {

      startBuzzer();
    }


    else if (c == 'o' || c == 'O') {

      allOutputsOff();

      Serial.println(">>> ALL OUTPUTS OFF");
    }


    else if (c == 'h' || c == 'H') {

      Serial.println();
      Serial.println("===== COMMAND HELP =====");
      Serial.println("G : Green LED");
      Serial.println("R : Red LED");
      Serial.println("V : Vibration 70 ms");
      Serial.println("B : Buzzer 100 ms");
      Serial.println("O : All outputs OFF");
      Serial.println("========================");
      Serial.println();
    }
  }
}


// =====================================================
// SENSOR PRINT
// =====================================================

void printStatus() {

  unsigned long now = millis();


  if (now - lastPrintTime < PRINT_INTERVAL_MS) {
    return;
  }

  lastPrintTime = now;


  bool imuReliable =
      ((long)(now - imuIgnoreUntil) >= 0);


  Serial.print("IMU=");

  if (imuReadOK) {
    Serial.print("OK");
  } else {
    Serial.print("ERR");
  }


  Serial.print(" | ROLL=");
  Serial.print(rollDeg, 2);

  Serial.print(" | PITCH=");
  Serial.print(pitchDeg, 2);


  Serial.print(" | IMU_VALID=");

  if (imuReadOK && imuReliable) {
    Serial.print("YES");
  } else {
    Serial.print("NO");
  }


  Serial.print(" | FSR_PAD=");
  Serial.print(fsrPadValue);


  Serial.print(" | FSR_PEN=");
  Serial.print(fsrPenValue);


  Serial.print(" | PLUNGER=");

  if (stablePlunger == LOW) {
    Serial.print("DOWN");
  } else {
    Serial.print("UP");
  }


  Serial.print(" | HOLD=");
  Serial.print(getCurrentHoldSec(), 2);
  Serial.print("s");


  Serial.print(" | GREEN=");
  Serial.print(
    digitalRead(GREEN_LED_PIN) ? "ON" : "OFF"
  );


  Serial.print(" | RED=");
  Serial.print(
    digitalRead(RED_LED_PIN) ? "ON" : "OFF"
  );


  Serial.print(" | VIB=");
  Serial.print(
    vibrationActive ? "ON" : "OFF"
  );


  Serial.print(" | BUZZER=");
  Serial.println(
    buzzerActive ? "ON" : "OFF"
  );
}


// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(115200);
  delay(500);


  // INPUT
  pinMode(FSR_PAD_PIN, INPUT);
  pinMode(FSR_PEN_PIN, INPUT);

  pinMode(PLUNGER_PIN, INPUT_PULLUP);


  // OUTPUT
  pinMode(VIBRATION_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);


  allOutputsOff();


  // Plunger initial state
  lastRawPlunger =
      digitalRead(PLUNGER_PIN);

  stablePlunger =
      lastRawPlunger;


  // I2C
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);


  Serial.println();
  Serial.println("======================================");
  Serial.println("KOKCHI FULL I/O INTEGRATION TEST");
  Serial.println("======================================");


  bool imuInitOK = initIMU();

  if (imuInitOK) {

    Serial.println("IMU INIT = OK");
  }

  else {

    Serial.println("IMU INIT = ERROR");
    Serial.println("Other sensors/outputs will still run.");
  }


  Serial.println();
  Serial.println("COMMANDS:");
  Serial.println("G = Green LED");
  Serial.println("R = Red LED");
  Serial.println("V = Vibration 70ms");
  Serial.println("B = Buzzer 100ms");
  Serial.println("O = All OFF");
  Serial.println("H = Help");
  Serial.println();

  Serial.println("START.");
}


// =====================================================
// LOOP
// =====================================================

void loop() {

  // 1. IMU
  imuReadOK =
      readIMU(rollDeg, pitchDeg);


  // 2. FSR
  fsrPadValue =
      analogRead(FSR_PAD_PIN);

  fsrPenValue =
      analogRead(FSR_PEN_PIN);


  // 3. Plunger / Hold
  updatePlunger();


  // 4. Outputs
  updateOutputs();


  // 5. Manual output commands
  handleSerialCommands();


  // 6. Serial status
  printStatus();
}