#include <Wire.h>
#include <math.h>

// =========================================================
// KOKCHI - Embedded State Machine Test v2
//
// FINAL GRIP REFERENCE
// - 표시된 GRIP 방향 유지
// - 주사기 축 = 몸/패드 표면과 수직
// - 주사기 축 = 지면과 평행
//
// INPUT
// IMU SDA     GPIO21
// IMU SCL     GPIO22
// FSR-PAD     GPIO32
// FSR-PEN     GPIO34
// Plunger     GPIO27
//
// OUTPUT
// Vibration   GPIO26
// Buzzer      GPIO25
// Green LED   GPIO33
// Red LED     GPIO14
// =========================================================


// =========================================================
// PIN MAP
// =========================================================

const int SDA_PIN = 21;
const int SCL_PIN = 22;

const int FSR_PAD_PIN = 32;
const int FSR_PEN_PIN = 34;
const int PLUNGER_PIN = 27;

const int VIBRATION_PIN = 26;
const int BUZZER_PIN = 25;
const int GREEN_LED_PIN = 33;
const int RED_LED_PIN = 14;


// =========================================================
// IMU
// =========================================================

const uint8_t IMU_ADDR = 0x68;


// =========================================================
// FINAL CALIBRATION PARAMETERS
// =========================================================

// FSR Contact Threshold
const int FSR_PAD_THRESHOLD = 2000;
const int FSR_PEN_THRESHOLD = 1500;


// FINAL marked-grip reference
const float TARGET_ROLL = -7.56;
const float TARGET_PITCH = 52.83;


// -----------------------------------------
// 수정된 Orientation 허용범위
// 기존 ±3° → ±4°
// -----------------------------------------

const float ROLL_TOLERANCE = 4.0;
const float PITCH_TOLERANCE = 4.0;


// -----------------------------------------
// 순간적인 손떨림 방지
//
// 기준 범위를 벗어나도
// 300ms 이상 계속 틀어져 있어야
// 실제 BAD_ORIENTATION으로 인정
// -----------------------------------------

const unsigned long BAD_ORIENTATION_CONFIRM_MS = 300;


// =========================================================
// FEEDBACK PARAMETERS
// =========================================================

const unsigned long VIBRATION_MS = 70;

// 진동 종료 후 IMU 안정화 보호시간
const unsigned long IMU_SETTLE_MS = 250;

// 완료음
const unsigned long BUZZER_MS = 100;


// =========================================================
// HOLD
// =========================================================

// Demo Profile 테스트용 값
// 의료적 절대 기준 아님
const float HOLD_TARGET_SEC = 3.0;


// =========================================================
// BUTTON / SERIAL
// =========================================================

const unsigned long DEBOUNCE_MS = 30;
const unsigned long PRINT_INTERVAL_MS = 200;


// =========================================================
// STATE MACHINE
// =========================================================

enum SystemState {

  WAIT_SITE,
  READY,
  ORIENTATION_CHECK,
  HOLDING,
  RESULT_SUCCESS,
  RESULT_FAIL
};

SystemState state = WAIT_SITE;


// =========================================================
// ISSUE
// =========================================================

enum IssueCode {

  ISSUE_NONE,
  PARTIAL_CONTACT,
  BAD_ORIENTATION,
  PLUNGER_TOO_EARLY,
  CONTACT_LOST,
  EARLY_RELEASE
};

IssueCode currentIssue = ISSUE_NONE;


// =========================================================
// SENSOR VALUES
// =========================================================

float rollDeg = 0.0;
float pitchDeg = 0.0;

bool imuReadOK = false;

int fsrPadValue = 0;
int fsrPenValue = 0;

bool padContact = false;
bool penContact = false;
bool contactConfirmed = false;

bool orientationGood = false;


// =========================================================
// ORIENTATION CONFIRM TIMER
// =========================================================

// 자세가 처음 틀어진 시간
unsigned long badOrientationStartMs = 0;

// 현재 BAD timer가 동작 중인지
bool badOrientationTiming = false;

// 300ms 이상 계속 틀어졌는지
bool badOrientationConfirmed = false;


// =========================================================
// PLUNGER
// =========================================================

bool lastRawPlunger = HIGH;
bool stablePlunger = HIGH;

bool plungerDownEvent = false;
bool plungerUpEvent = false;

unsigned long debounceStart = 0;


// =========================================================
// HOLD
// =========================================================

unsigned long validHoldAccumMs = 0;
unsigned long lastHoldUpdateMs = 0;


// =========================================================
// OUTPUT TIMERS
// =========================================================

bool vibrationActive = false;
bool buzzerActive = false;

unsigned long vibrationEndMs = 0;
unsigned long buzzerEndMs = 0;

unsigned long imuIgnoreUntil = 0;


// =========================================================
// GENERAL
// =========================================================

unsigned long lastPrintMs = 0;


// =========================================================
// IMU INITIALIZATION
// =========================================================

bool initIMU() {

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

  if (whoAmI < 0x10) {
    Serial.print("0");
  }

  Serial.println(whoAmI, HEX);


  if (whoAmI == 0x00 ||
      whoAmI == 0xFF) {

    return false;
  }


  return true;
}


// =========================================================
// READ IMU
// =========================================================

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
      ((int16_t)Wire.read() << 8) |
      Wire.read();

  int16_t rawAy =
      ((int16_t)Wire.read() << 8) |
      Wire.read();

  int16_t rawAz =
      ((int16_t)Wire.read() << 8) |
      Wire.read();


  float ax = rawAx / 16384.0;
  float ay = rawAy / 16384.0;
  float az = rawAz / 16384.0;


  roll =
      atan2(ay, az) *
      180.0 / PI;


  pitch =
      atan2(
        -ax,
        sqrt(
          ay * ay +
          az * az
        )
      ) *
      180.0 / PI;


  return true;
}


// =========================================================
// IMU DECISION VALID?
// =========================================================

bool imuDecisionValid() {

  if (!imuReadOK) {
    return false;
  }


  return
    (long)(
      millis() -
      imuIgnoreUntil
    ) >= 0;
}


// =========================================================
// RAW ORIENTATION CHECK
// =========================================================

bool checkOrientationRaw() {

  float deltaRoll =
      fabs(
        rollDeg -
        TARGET_ROLL
      );

  float deltaPitch =
      fabs(
        pitchDeg -
        TARGET_PITCH
      );


  return
    deltaRoll <= ROLL_TOLERANCE &&
    deltaPitch <= PITCH_TOLERANCE;
}


// =========================================================
// ORIENTATION STABILITY FILTER
//
// 핵심 수정 부분
//
// 순간적으로 기준 밖으로 나가도
// 바로 BAD로 판정하지 않는다.
//
// 300ms 이상 연속해서 기준 밖일 때만
// BAD_ORIENTATION 확정.
// =========================================================

bool updateOrientationDecision() {

  unsigned long now = millis();


  bool rawGood =
      checkOrientationRaw();


  // -----------------------------------------
  // 정상 자세
  // -----------------------------------------

  if (rawGood) {

    // BAD timer 초기화
    badOrientationTiming = false;
    badOrientationConfirmed = false;

    return true;
  }


  // -----------------------------------------
  // 처음 기준을 벗어남
  // -----------------------------------------

  if (!badOrientationTiming) {

    badOrientationTiming = true;

    badOrientationStartMs = now;

    badOrientationConfirmed = false;


    // 아직은 오류 확정하지 않음
    return true;
  }


  // -----------------------------------------
  // 계속 기준 밖
  // -----------------------------------------

  if (
    now -
    badOrientationStartMs
    >=
    BAD_ORIENTATION_CONFIRM_MS
  ) {

    badOrientationConfirmed = true;

    return false;
  }


  // 300ms 미만이면
  // 잠깐의 흔들림으로 보고 정상 유지
  return true;
}


// =========================================================
// RESET ORIENTATION FILTER
// =========================================================

void resetOrientationFilter() {

  badOrientationTiming = false;

  badOrientationConfirmed = false;

  badOrientationStartMs = 0;
}


// =========================================================
// LED FUNCTIONS
// =========================================================

void ledsOff() {

  digitalWrite(
    GREEN_LED_PIN,
    LOW
  );

  digitalWrite(
    RED_LED_PIN,
    LOW
  );
}


void showGreen() {

  digitalWrite(
    GREEN_LED_PIN,
    HIGH
  );

  digitalWrite(
    RED_LED_PIN,
    LOW
  );
}


void showRed() {

  digitalWrite(
    GREEN_LED_PIN,
    LOW
  );

  digitalWrite(
    RED_LED_PIN,
    HIGH
  );
}


// =========================================================
// VIBRATION
// =========================================================

void startVibration() {

  if (vibrationActive) {
    return;
  }


  unsigned long now =
      millis();


  digitalWrite(
    VIBRATION_PIN,
    HIGH
  );


  vibrationActive = true;


  vibrationEndMs =
      now +
      VIBRATION_MS;


  // 진동 70ms
  // +
  // IMU 안정화 250ms
  imuIgnoreUntil =
      now +
      VIBRATION_MS +
      IMU_SETTLE_MS;


  Serial.println(
    ">>> VIBRATION WARNING"
  );
}


// =========================================================
// BUZZER
// =========================================================

void startBuzzer() {

  if (buzzerActive) {
    return;
  }


  unsigned long now =
      millis();


  digitalWrite(
    BUZZER_PIN,
    HIGH
  );


  buzzerActive = true;


  buzzerEndMs =
      now +
      BUZZER_MS;


  Serial.println(
    ">>> COMPLETION BEEP"
  );
}


// =========================================================
// UPDATE OUTPUTS
// =========================================================

void updateOutputs() {

  unsigned long now =
      millis();


  // Vibration OFF
  if (
    vibrationActive &&
    (long)(
      now -
      vibrationEndMs
    ) >= 0
  ) {

    digitalWrite(
      VIBRATION_PIN,
      LOW
    );

    vibrationActive = false;

    Serial.println(
      "<<< VIBRATION OFF"
    );
  }


  // Buzzer OFF
  if (
    buzzerActive &&
    (long)(
      now -
      buzzerEndMs
    ) >= 0
  ) {

    digitalWrite(
      BUZZER_PIN,
      LOW
    );

    buzzerActive = false;

    Serial.println(
      "<<< BUZZER OFF"
    );
  }
}


// =========================================================
// ISSUE CONTROL
// =========================================================

void setIssue(
  IssueCode newIssue
) {

  // 같은 오류 유지 중이면
  // 진동 반복하지 않음
  if (
    newIssue ==
    currentIssue
  ) {

    return;
  }


  currentIssue =
      newIssue;


  if (
    newIssue ==
    ISSUE_NONE
  ) {

    return;
  }


  // 새 오류 발생 시
  // 진동 1회
  startVibration();
}


// =========================================================
// ISSUE NAME
// =========================================================

const char* issueName(
  IssueCode issue
) {

  switch (issue) {

    case ISSUE_NONE:
      return "NONE";

    case PARTIAL_CONTACT:
      return "PARTIAL_CONTACT";

    case BAD_ORIENTATION:
      return "BAD_ORIENTATION";

    case PLUNGER_TOO_EARLY:
      return "PLUNGER_TOO_EARLY";

    case CONTACT_LOST:
      return "CONTACT_LOST";

    case EARLY_RELEASE:
      return "EARLY_RELEASE";

    default:
      return "UNKNOWN";
  }
}


// =========================================================
// STATE NAME
// =========================================================

const char* stateName(
  SystemState s
) {

  switch (s) {

    case WAIT_SITE:
      return "WAIT_SITE";

    case READY:
      return "READY";

    case ORIENTATION_CHECK:
      return "ORIENTATION_CHECK";

    case HOLDING:
      return "HOLDING";

    case RESULT_SUCCESS:
      return "RESULT_SUCCESS";

    case RESULT_FAIL:
      return "RESULT_FAIL";

    default:
      return "UNKNOWN";
  }
}


// =========================================================
// STATE CHANGE
// =========================================================

void setState(
  SystemState newState
) {

  if (
    state ==
    newState
  ) {

    return;
  }


  state =
      newState;


  Serial.println();

  Serial.print(
    "=== STATE -> "
  );

  Serial.print(
    stateName(state)
  );

  Serial.println(
    " ==="
  );


  // -----------------------------------------
  // WAIT SITE
  // -----------------------------------------

  if (
    state ==
    WAIT_SITE
  ) {

    ledsOff();

    currentIssue =
        ISSUE_NONE;

    validHoldAccumMs = 0;

    resetOrientationFilter();
  }


  // -----------------------------------------
  // READY
  // -----------------------------------------

  else if (
    state ==
    READY
  ) {

    ledsOff();

    currentIssue =
        ISSUE_NONE;

    validHoldAccumMs = 0;

    resetOrientationFilter();
  }


  // -----------------------------------------
  // ORIENTATION CHECK
  // -----------------------------------------

  else if (
    state ==
    ORIENTATION_CHECK
  ) {

    currentIssue =
        ISSUE_NONE;

    resetOrientationFilter();
  }


  // -----------------------------------------
  // HOLDING
  // -----------------------------------------

  else if (
    state ==
    HOLDING
  ) {

    validHoldAccumMs = 0;

    lastHoldUpdateMs =
        millis();

    currentIssue =
        ISSUE_NONE;

    resetOrientationFilter();

    showGreen();


    Serial.println(
      ">>> HOLD TIMER START"
    );
  }


  // -----------------------------------------
  // SUCCESS
  // -----------------------------------------

  else if (
    state ==
    RESULT_SUCCESS
  ) {

    currentIssue =
        ISSUE_NONE;

    showGreen();

    startBuzzer();


    Serial.println();

    Serial.println(
      "========================="
    );

    Serial.println(
      "      SESSION PASS"
    );

    Serial.println(
      "========================="
    );
  }


  // -----------------------------------------
  // FAIL
  // -----------------------------------------

  else if (
    state ==
    RESULT_FAIL
  ) {

    showRed();

    startVibration();


    Serial.println();

    Serial.println(
      "========================="
    );

    Serial.println(
      "      SESSION FAIL"
    );

    Serial.println(
      "========================="
    );
  }
}


// =========================================================
// PLUNGER
// =========================================================

void updatePlunger() {

  plungerDownEvent = false;
  plungerUpEvent = false;


  unsigned long now =
      millis();


  bool raw =
      digitalRead(
        PLUNGER_PIN
      );


  if (
    raw !=
    lastRawPlunger
  ) {

    lastRawPlunger =
        raw;

    debounceStart =
        now;
  }


  if (
    now -
    debounceStart
    >=
    DEBOUNCE_MS
    &&
    raw !=
    stablePlunger
  ) {

    stablePlunger =
        raw;


    // DOWN
    if (
      stablePlunger ==
      LOW
    ) {

      plungerDownEvent =
          true;


      Serial.println(
        ">>> PLUNGER DOWN"
      );
    }


    // RELEASE
    else {

      plungerUpEvent =
          true;


      Serial.println(
        "<<< PLUNGER RELEASE"
      );
    }
  }
}


// =========================================================
// VALID HOLD
// =========================================================

float getValidHoldSec() {

  return
    validHoldAccumMs /
    1000.0;
}


// =========================================================
// STATE MACHINE
// =========================================================

void updateStateMachine() {

  unsigned long now =
      millis();


  // =====================================================
  // WAIT SITE
  // =====================================================

  if (
    state ==
    WAIT_SITE
  ) {

    ledsOff();

    return;
  }


  // =====================================================
  // READY
  // =====================================================

  if (
    state ==
    READY
  ) {

    // 아무 접촉 없음
    if (
      !padContact &&
      !penContact
    ) {

      ledsOff();

      setIssue(
        ISSUE_NONE
      );

      return;
    }


    // 하나만 접촉
    if (
      padContact !=
      penContact
    ) {

      showRed();

      setIssue(
        PARTIAL_CONTACT
      );

      return;
    }


    // 둘 다 CONTACT
    if (
      contactConfirmed
    ) {

      setIssue(
        ISSUE_NONE
      );

      setState(
        ORIENTATION_CHECK
      );

      return;
    }
  }


  // =====================================================
  // ORIENTATION CHECK
  // =====================================================

  else if (
    state ==
    ORIENTATION_CHECK
  ) {

    // Contact 사라짐
    if (
      !contactConfirmed
    ) {

      resetOrientationFilter();

      setIssue(
        ISSUE_NONE
      );

      setState(
        READY
      );

      return;
    }


    // 진동 후 IMU 안정화 중
    if (
      !imuDecisionValid()
    ) {

      return;
    }


    // -----------------------------------------
    // 새 filtered orientation 판단
    // -----------------------------------------

    orientationGood =
        updateOrientationDecision();


    // -----------------------------------------
    // 300ms 이상 확실히 BAD
    // -----------------------------------------

    if (
      !orientationGood
    ) {

      showRed();

      setIssue(
        BAD_ORIENTATION
      );


      // BAD 상태에서
      // Plunger를 너무 일찍 누름
      if (
        plungerDownEvent
      ) {

        setIssue(
          PLUNGER_TOO_EARLY
        );
      }


      return;
    }


    // -----------------------------------------
    // 정상 또는 순간적인 흔들림
    // -----------------------------------------

    showGreen();

    setIssue(
      ISSUE_NONE
    );


    // 정상 상태에서
    // Plunger DOWN
    if (
      plungerDownEvent
    ) {

      setState(
        HOLDING
      );

      return;
    }
  }


  // =====================================================
  // HOLDING
  // =====================================================

  else if (
    state ==
    HOLDING
  ) {

    unsigned long dt =
        now -
        lastHoldUpdateMs;


    lastHoldUpdateMs =
        now;


    // -----------------------------------------
    // Contact loss
    // -----------------------------------------

    if (
      !contactConfirmed
    ) {

      setIssue(
        CONTACT_LOST
      );

      setState(
        RESULT_FAIL
      );

      return;
    }


    // -----------------------------------------
    // Plunger RELEASE
    // -----------------------------------------

    if (
      plungerUpEvent
    ) {

      if (
        getValidHoldSec()
        >=
        HOLD_TARGET_SEC
      ) {

        setState(
          RESULT_SUCCESS
        );
      }

      else {

        setIssue(
          EARLY_RELEASE
        );

        setState(
          RESULT_FAIL
        );
      }


      return;
    }


    // -----------------------------------------
    // IMU 안정화 중
    // -----------------------------------------

    if (
      !imuDecisionValid()
    ) {

      return;
    }


    // -----------------------------------------
    // Filtered orientation 판단
    // -----------------------------------------

    orientationGood =
        updateOrientationDecision();


    // -----------------------------------------
    // 300ms 이상 자세 오류
    // -----------------------------------------

    if (
      !orientationGood
    ) {

      showRed();

      setIssue(
        BAD_ORIENTATION
      );


      // 오류가 확정된 동안에는
      // Hold 시간 누적하지 않음
      return;
    }


    // -----------------------------------------
    // 정상 또는 짧은 손떨림
    // -----------------------------------------

    showGreen();

    setIssue(
      ISSUE_NONE
    );


    // 정상 조건에서만
    // Hold 시간 누적
    if (
      stablePlunger ==
      LOW
    ) {

      validHoldAccumMs +=
          dt;
    }
  }


  // =====================================================
  // RESULT
  // =====================================================

  else if (
    state ==
    RESULT_SUCCESS
    ||
    state ==
    RESULT_FAIL
  ) {

    return;
  }
}


// =========================================================
// SERIAL COMMAND
// =========================================================

void handleSerialCommands() {

  while (
    Serial.available()
  ) {

    char c =
        Serial.read();


    // -----------------------------------------
    // S = Web의 "이 위치로 시작"
    // -----------------------------------------

    if (
      c == 's' ||
      c == 'S'
    ) {

      if (
        state ==
        WAIT_SITE
      ) {

        Serial.println();

        Serial.println(
          ">>> SITE SELECTED / SESSION START"
        );

        setState(
          READY
        );
      }
    }


    // -----------------------------------------
    // N = New session
    // -----------------------------------------

    else if (
      c == 'n' ||
      c == 'N'
    ) {

      Serial.println();

      Serial.println(
        ">>> NEW SESSION"
      );


      digitalWrite(
        VIBRATION_PIN,
        LOW
      );

      digitalWrite(
        BUZZER_PIN,
        LOW
      );


      vibrationActive = false;
      buzzerActive = false;


      setState(
        WAIT_SITE
      );
    }


    // -----------------------------------------
    // H = Help
    // -----------------------------------------

    else if (
      c == 'h' ||
      c == 'H'
    ) {

      Serial.println();

      Serial.println(
        "COMMANDS:"
      );

      Serial.println(
        "S = Start session / site selected"
      );

      Serial.println(
        "N = New session"
      );

      Serial.println(
        "H = Help"
      );

      Serial.println();
    }
  }
}


// =========================================================
// STATUS PRINT
// =========================================================

void printStatus() {

  unsigned long now =
      millis();


  if (
    now -
    lastPrintMs
    <
    PRINT_INTERVAL_MS
  ) {

    return;
  }


  lastPrintMs =
      now;


  float deltaRoll =
      fabs(
        rollDeg -
        TARGET_ROLL
      );

  float deltaPitch =
      fabs(
        pitchDeg -
        TARGET_PITCH
      );


  Serial.print(
    "STATE="
  );

  Serial.print(
    stateName(state)
  );


  // PAD
  Serial.print(
    " | PAD="
  );

  Serial.print(
    fsrPadValue
  );

  Serial.print("(");

  Serial.print(
    padContact
      ? "ON"
      : "OFF"
  );

  Serial.print(")");


  // PEN
  Serial.print(
    " | PEN="
  );

  Serial.print(
    fsrPenValue
  );

  Serial.print("(");

  Serial.print(
    penContact
      ? "ON"
      : "OFF"
  );

  Serial.print(")");


  // Roll
  Serial.print(
    " | ROLL="
  );

  Serial.print(
    rollDeg,
    2
  );


  // Pitch
  Serial.print(
    " | PITCH="
  );

  Serial.print(
    pitchDeg,
    2
  );


  // Delta
  Serial.print(
    " | dR="
  );

  Serial.print(
    deltaRoll,
    2
  );


  Serial.print(
    " | dP="
  );

  Serial.print(
    deltaPitch,
    2
  );


  // IMU valid
  Serial.print(
    " | IMU_VALID="
  );

  Serial.print(
    imuDecisionValid()
      ? "YES"
      : "NO"
  );


  // Angle
  Serial.print(
    " | ANGLE="
  );

  if (
    badOrientationConfirmed
  ) {

    Serial.print(
      "BAD"
    );
  }

  else if (
    badOrientationTiming
  ) {

    Serial.print(
      "CHECKING"
    );
  }

  else {

    Serial.print(
      "GOOD"
    );
  }


  // Plunger
  Serial.print(
    " | PLUNGER="
  );

  Serial.print(
    stablePlunger ==
    LOW
      ? "DOWN"
      : "UP"
  );


  // Hold
  Serial.print(
    " | VALID_HOLD="
  );

  Serial.print(
    getValidHoldSec(),
    2
  );

  Serial.print("/");

  Serial.print(
    HOLD_TARGET_SEC,
    1
  );

  Serial.print("s");


  // Issue
  Serial.print(
    " | ISSUE="
  );

  Serial.println(
    issueName(
      currentIssue
    )
  );
}


// =========================================================
// SETUP
// =========================================================

void setup() {

  Serial.begin(
    115200
  );

  delay(500);


  analogReadResolution(
    12
  );


  // INPUT
  pinMode(
    FSR_PAD_PIN,
    INPUT
  );

  pinMode(
    FSR_PEN_PIN,
    INPUT
  );

  pinMode(
    PLUNGER_PIN,
    INPUT_PULLUP
  );


  // OUTPUT
  pinMode(
    VIBRATION_PIN,
    OUTPUT
  );

  pinMode(
    BUZZER_PIN,
    OUTPUT
  );

  pinMode(
    GREEN_LED_PIN,
    OUTPUT
  );

  pinMode(
    RED_LED_PIN,
    OUTPUT
  );


  digitalWrite(
    VIBRATION_PIN,
    LOW
  );

  digitalWrite(
    BUZZER_PIN,
    LOW
  );

  ledsOff();


  // Plunger initial
  lastRawPlunger =
      digitalRead(
        PLUNGER_PIN
      );

  stablePlunger =
      lastRawPlunger;


  // I2C
  Wire.begin(
    SDA_PIN,
    SCL_PIN
  );

  Wire.setClock(
    400000
  );


  Serial.println();

  Serial.println(
    "========================================"
  );

  Serial.println(
    "KOKCHI EMBEDDED STATE MACHINE TEST v2"
  );

  Serial.println(
    "========================================"
  );


  if (
    initIMU()
  ) {

    Serial.println(
      "IMU INIT = OK"
    );
  }

  else {

    Serial.println(
      "IMU INIT = ERROR"
    );


    while (true) {

      digitalWrite(
        VIBRATION_PIN,
        LOW
      );

      digitalWrite(
        BUZZER_PIN,
        LOW
      );

      delay(1000);
    }
  }


  Serial.println();

  Serial.println(
    "FINAL PARAMETERS"
  );


  Serial.print(
    "FSR_PAD_THRESHOLD = "
  );

  Serial.println(
    FSR_PAD_THRESHOLD
  );


  Serial.print(
    "FSR_PEN_THRESHOLD = "
  );

  Serial.println(
    FSR_PEN_THRESHOLD
  );


  Serial.print(
    "TARGET_ROLL = "
  );

  Serial.println(
    TARGET_ROLL,
    2
  );


  Serial.print(
    "TARGET_PITCH = "
  );

  Serial.println(
    TARGET_PITCH,
    2
  );


  Serial.print(
    "ROLL_TOLERANCE = +/- "
  );

  Serial.println(
    ROLL_TOLERANCE,
    1
  );


  Serial.print(
    "PITCH_TOLERANCE = +/- "
  );

  Serial.println(
    PITCH_TOLERANCE,
    1
  );


  Serial.print(
    "BAD CONFIRM = "
  );

  Serial.print(
    BAD_ORIENTATION_CONFIRM_MS
  );

  Serial.println(
    " ms"
  );


  Serial.print(
    "HOLD_TARGET = "
  );

  Serial.print(
    HOLD_TARGET_SEC,
    1
  );

  Serial.println(
    " sec (DEMO)"
  );


  Serial.println();

  Serial.println(
    "COMMAND:"
  );

  Serial.println(
    "S = SITE SELECTED / START"
  );

  Serial.println(
    "N = NEW SESSION"
  );


  Serial.println();

  Serial.println(
    "Waiting for S..."
  );


  setState(
    WAIT_SITE
  );
}


// =========================================================
// LOOP
// =========================================================

void loop() {

  // 1. IMU
  imuReadOK =
      readIMU(
        rollDeg,
        pitchDeg
      );


  // 2. FSR
  fsrPadValue =
      analogRead(
        FSR_PAD_PIN
      );

  fsrPenValue =
      analogRead(
        FSR_PEN_PIN
      );


  padContact =
      fsrPadValue >=
      FSR_PAD_THRESHOLD;


  penContact =
      fsrPenValue >=
      FSR_PEN_THRESHOLD;


  contactConfirmed =
      padContact &&
      penContact;


  // 3. Plunger
  updatePlunger();


  // 4. Feedback timer
  updateOutputs();


  // 5. Serial
  handleSerialCommands();


  // 6. State Machine
  updateStateMachine();


  // 7. Status
  printStatus();
}