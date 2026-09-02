#include <Wire.h>
#include <math.h>

// =========================================================
// KOKCHI - State Machine Test
// 2026-09-02
//
// BEFORE PLUNGER
//   - Check configured reference orientation
//   - Persistent orientation deviation: Red + Vibration
//
// AFTER PLUNGER DOWN
//   - Capture pose at PLUNGER DOWN
//   - Monitor drift from captured pose
//   - Drift warning: Red LED only
//   - No vibration during HOLD
//   - Hold timer continues during warning
//
// HOLD COMPLETE
//   - Green LED
//   - Buzzer notification
//   - Wait for plunger release
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

const uint8_t IMU_ADDR = 0x68;


// =========================================================
// CONTACT THRESHOLDS
// =========================================================

const int FSR_PAD_THRESHOLD = 2000;
const int FSR_PEN_THRESHOLD = 1500;


// =========================================================
// PRE-INJECTION ORIENTATION
// =========================================================

const float TARGET_ROLL  = -7.56f;
const float TARGET_PITCH = 52.83f;

const float PRE_ROLL_TOLERANCE  = 8.0f;
const float PRE_PITCH_TOLERANCE = 8.0f;

const unsigned long PRE_BAD_CONFIRM_MS = 400;


// =========================================================
// DURING-HOLD STABILITY
// =========================================================

// Reference = pose captured at valid PLUNGER DOWN.

const float HOLD_DRIFT_WARN_ROLL  = 7.0f;
const float HOLD_DRIFT_WARN_PITCH = 7.0f;

// Persistent deviation before Red warning.
const unsigned long HOLD_DRIFT_WARN_CONFIRM_MS = 250;

// Recovery threshold for hysteresis.
const float HOLD_DRIFT_CLEAR_ROLL  = 5.5f;
const float HOLD_DRIFT_CLEAR_PITCH = 5.5f;

// Stable recovery before Green is restored.
const unsigned long HOLD_DRIFT_CLEAR_CONFIRM_MS = 200;


// =========================================================
// FEEDBACK / SYSTEM PARAMETERS
// =========================================================

const unsigned long VIBRATION_MS = 70;
const unsigned long IMU_SETTLE_MS = 250;

const unsigned long BUZZER_MS = 350;

const unsigned long DEBOUNCE_MS = 30;
const unsigned long PRINT_INTERVAL_MS = 200;


// =========================================================
// HOLD CONFIGURATION
// =========================================================

unsigned long holdTargetMs = 3000;


// =========================================================
// STATE MACHINE
// =========================================================

enum SystemState {
  WAIT_SITE,
  READY,
  ORIENTATION_CHECK,
  HOLDING,
  HOLD_COMPLETE,
  RESULT_SUCCESS,
  RESULT_INTERRUPTED
};

SystemState state = WAIT_SITE;


// =========================================================
// ISSUE
// =========================================================

enum IssueCode {
  ISSUE_NONE,
  PARTIAL_CONTACT,
  BAD_ORIENTATION_PRE,
  PLUNGER_TOO_EARLY,
  HOLD_MOVEMENT,
  EARLY_RELEASE
};

IssueCode currentIssue = ISSUE_NONE;


// =========================================================
// SENSOR VALUES
// =========================================================

float rollDeg = 0.0f;
float pitchDeg = 0.0f;

bool imuReadOK = false;

int fsrPadValue = 0;
int fsrPenValue = 0;

bool padContact = false;
bool penContact = false;
bool contactConfirmed = false;


// =========================================================
// PRE-ORIENTATION FILTER
// =========================================================

bool preBadTiming = false;
bool preBadConfirmed = false;

unsigned long preBadStartMs = 0;


// =========================================================
// HOLD REFERENCE / DRIFT
// =========================================================

float holdStartRoll = 0.0f;
float holdStartPitch = 0.0f;

bool holdDriftWarning = false;

bool holdDriftBadTiming = false;
unsigned long holdDriftBadStartMs = 0;

bool holdDriftClearTiming = false;
unsigned long holdDriftClearStartMs = 0;

int holdDriftWarningCount = 0;

float maxHoldDriftRoll = 0.0f;
float maxHoldDriftPitch = 0.0f;


// =========================================================
// PLUNGER
// =========================================================

bool lastRawPlunger = HIGH;
bool stablePlunger = HIGH;

bool plungerDownEvent = false;
bool plungerUpEvent = false;

bool pendingPlungerStart = false;

unsigned long debounceStartMs = 0;


// =========================================================
// HOLD TIMER
// =========================================================

unsigned long holdStartMs = 0;
unsigned long holdElapsedMs = 0;


// =========================================================
// OUTPUT TIMERS
// =========================================================

bool vibrationActive = false;
bool buzzerActive = false;

unsigned long vibrationEndMs = 0;
unsigned long buzzerEndMs = 0;

unsigned long imuIgnoreUntilMs = 0;


// =========================================================
// SERIAL INPUT
// =========================================================

bool serialWaitingForZero = false;


// =========================================================
// GENERAL
// =========================================================

unsigned long lastPrintMs = 0;


// =========================================================
// ANGLE DIFFERENCE
// =========================================================

float angleDiffAbs(
  float a,
  float b
) {
  float d =
      fmodf(
        a - b + 540.0f,
        360.0f
      ) - 180.0f;

  return fabsf(d);
}


// =========================================================
// IMU INIT
// =========================================================

bool initIMU() {

  Wire.beginTransmission(IMU_ADDR);
  Wire.write(0x6B);
  Wire.write(0x00);

  if (
    Wire.endTransmission() != 0
  ) {
    return false;
  }

  delay(100);

  Wire.beginTransmission(IMU_ADDR);
  Wire.write(0x75);

  if (
    Wire.endTransmission(false) != 0
  ) {
    return false;
  }

  Wire.requestFrom(
    IMU_ADDR,
    (uint8_t)1
  );

  if (!Wire.available()) {
    return false;
  }

  uint8_t whoAmI =
      Wire.read();

  Serial.print("WHO_AM_I = 0x");

  if (whoAmI < 0x10) {
    Serial.print("0");
  }

  Serial.println(
    whoAmI,
    HEX
  );

  if (
    whoAmI == 0x00 ||
    whoAmI == 0xFF
  ) {
    return false;
  }

  return true;
}


// =========================================================
// IMU READ
// =========================================================

bool readIMU(
  float &roll,
  float &pitch
) {

  Wire.beginTransmission(IMU_ADDR);
  Wire.write(0x3B);

  if (
    Wire.endTransmission(false) != 0
  ) {
    return false;
  }

  Wire.requestFrom(
    IMU_ADDR,
    (uint8_t)6
  );

  if (
    Wire.available() < 6
  ) {
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

  float ax =
      rawAx / 16384.0f;

  float ay =
      rawAy / 16384.0f;

  float az =
      rawAz / 16384.0f;

  roll =
      atan2f(
        ay,
        az
      )
      *
      180.0f / PI;

  pitch =
      atan2f(
        -ax,
        sqrtf(
          ay * ay +
          az * az
        )
      )
      *
      180.0f / PI;

  return true;
}


// =========================================================
// IMU DECISION VALID
// =========================================================

bool imuDecisionValid() {

  if (!imuReadOK) {
    return false;
  }

  return
    (long)(
      millis() -
      imuIgnoreUntilMs
    ) >= 0;
}


// =========================================================
// PRE-INJECTION ORIENTATION
// =========================================================

bool preOrientationRawGood() {

  float dRoll =
      angleDiffAbs(
        rollDeg,
        TARGET_ROLL
      );

  float dPitch =
      angleDiffAbs(
        pitchDeg,
        TARGET_PITCH
      );

  return
    dRoll <= PRE_ROLL_TOLERANCE
    &&
    dPitch <= PRE_PITCH_TOLERANCE;
}


// =========================================================
// PRE-ORIENTATION FILTER
// =========================================================

bool updatePreOrientation() {

  unsigned long now =
      millis();

  bool rawGood =
      preOrientationRawGood();

  if (rawGood) {

    preBadTiming = false;
    preBadConfirmed = false;
    preBadStartMs = 0;

    return true;
  }

  if (!preBadTiming) {

    preBadTiming = true;
    preBadConfirmed = false;
    preBadStartMs = now;

    return true;
  }

  if (
    now - preBadStartMs
    >=
    PRE_BAD_CONFIRM_MS
  ) {

    preBadConfirmed = true;

    return false;
  }

  return true;
}


void resetPreOrientationFilter() {

  preBadTiming = false;
  preBadConfirmed = false;

  preBadStartMs = 0;
}


// =========================================================
// HOLD DRIFT FILTER
// =========================================================

void resetHoldDriftFilter() {

  holdDriftWarning = false;

  holdDriftBadTiming = false;
  holdDriftBadStartMs = 0;

  holdDriftClearTiming = false;
  holdDriftClearStartMs = 0;

  holdDriftWarningCount = 0;

  maxHoldDriftRoll = 0.0f;
  maxHoldDriftPitch = 0.0f;
}


// =========================================================
// HOLD DRIFT UPDATE
// =========================================================

void updateHoldDrift() {

  unsigned long now =
      millis();

  float dRoll =
      angleDiffAbs(
        rollDeg,
        holdStartRoll
      );

  float dPitch =
      angleDiffAbs(
        pitchDeg,
        holdStartPitch
      );


  // -----------------------------------------------------
  // MAXIMUM DRIFT
  // -----------------------------------------------------

  if (
    dRoll >
    maxHoldDriftRoll
  ) {
    maxHoldDriftRoll =
        dRoll;
  }

  if (
    dPitch >
    maxHoldDriftPitch
  ) {
    maxHoldDriftPitch =
        dPitch;
  }


  // -----------------------------------------------------
  // NO WARNING ACTIVE
  // -----------------------------------------------------

  if (!holdDriftWarning) {

    bool beyondWarn =
        dRoll >
          HOLD_DRIFT_WARN_ROLL
        ||
        dPitch >
          HOLD_DRIFT_WARN_PITCH;

    if (beyondWarn) {

      if (!holdDriftBadTiming) {

        holdDriftBadTiming =
            true;

        holdDriftBadStartMs =
            now;
      }

      else if (
        now -
        holdDriftBadStartMs
        >=
        HOLD_DRIFT_WARN_CONFIRM_MS
      ) {

        holdDriftWarning =
            true;

        holdDriftBadTiming =
            false;

        holdDriftClearTiming =
            false;

        holdDriftWarningCount++;

        currentIssue =
            HOLD_MOVEMENT;

        Serial.println(
          ">>> HOLD MOVEMENT WARNING"
        );
      }
    }

    else {

      holdDriftBadTiming =
          false;

      holdDriftBadStartMs =
          0;
    }
  }


  // -----------------------------------------------------
  // WARNING ACTIVE
  // -----------------------------------------------------

  else {

    bool insideClear =
        dRoll <=
          HOLD_DRIFT_CLEAR_ROLL
        &&
        dPitch <=
          HOLD_DRIFT_CLEAR_PITCH;

    if (insideClear) {

      if (!holdDriftClearTiming) {

        holdDriftClearTiming =
            true;

        holdDriftClearStartMs =
            now;
      }

      else if (
        now -
        holdDriftClearStartMs
        >=
        HOLD_DRIFT_CLEAR_CONFIRM_MS
      ) {

        holdDriftWarning =
            false;

        holdDriftClearTiming =
            false;

        holdDriftBadTiming =
            false;

        currentIssue =
            ISSUE_NONE;

        Serial.println(
          "<<< HOLD STABILITY RESTORED"
        );
      }
    }

    else {

      holdDriftClearTiming =
          false;

      holdDriftClearStartMs =
          0;
    }
  }
}


// =========================================================
// LED
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

  vibrationActive =
      true;

  vibrationEndMs =
      now +
      VIBRATION_MS;

  imuIgnoreUntilMs =
      now +
      VIBRATION_MS +
      IMU_SETTLE_MS;

  Serial.println(
    ">>> VIBRATION WARNING"
  );
}


void stopVibration() {

  digitalWrite(
    VIBRATION_PIN,
    LOW
  );

  vibrationActive =
      false;
}


// =========================================================
// BUZZER
// =========================================================

void startBuzzer() {

  if (buzzerActive) {
    return;
  }

  digitalWrite(
    BUZZER_PIN,
    HIGH
  );

  buzzerActive =
      true;

  buzzerEndMs =
      millis() +
      BUZZER_MS;

  Serial.println(
    ">>> HOLD COMPLETE BEEP"
  );
}


// =========================================================
// OUTPUT UPDATE
// =========================================================

void updateOutputs() {

  unsigned long now =
      millis();

  if (
    vibrationActive
    &&
    (long)(
      now -
      vibrationEndMs
    ) >= 0
  ) {

    digitalWrite(
      VIBRATION_PIN,
      LOW
    );

    vibrationActive =
        false;
  }


  if (
    buzzerActive
    &&
    (long)(
      now -
      buzzerEndMs
    ) >= 0
  ) {

    digitalWrite(
      BUZZER_PIN,
      LOW
    );

    buzzerActive =
        false;
  }
}


// =========================================================
// ISSUE CONTROL
// =========================================================

void setIssue(
  IssueCode newIssue,
  bool vibrateOnNewIssue = false
) {

  if (
    newIssue ==
    currentIssue
  ) {
    return;
  }

  currentIssue =
      newIssue;

  if (
    newIssue != ISSUE_NONE
    &&
    vibrateOnNewIssue
  ) {

    startVibration();
  }
}


// =========================================================
// PLUNGER
// =========================================================

void updatePlunger() {

  plungerDownEvent =
      false;

  plungerUpEvent =
      false;

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

    debounceStartMs =
        now;
  }

  if (
    now -
    debounceStartMs
    >=
    DEBOUNCE_MS
    &&
    raw !=
    stablePlunger
  ) {

    stablePlunger =
        raw;

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

    case HOLD_COMPLETE:
      return "HOLD_COMPLETE";

    case RESULT_SUCCESS:
      return "RESULT_SUCCESS";

    case RESULT_INTERRUPTED:
      return "RESULT_INTERRUPTED";

    default:
      return "UNKNOWN";
  }
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

    case BAD_ORIENTATION_PRE:
      return "BAD_ORIENTATION_PRE";

    case PLUNGER_TOO_EARLY:
      return "PLUNGER_TOO_EARLY";

    case HOLD_MOVEMENT:
      return "HOLD_MOVEMENT";

    case EARLY_RELEASE:
      return "EARLY_RELEASE";

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


  // -----------------------------------------------------
  // READY
  // -----------------------------------------------------

  if (
    state ==
    READY
  ) {

    setIssue(
      ISSUE_NONE
    );

    pendingPlungerStart =
        false;

    ledsOff();

    resetPreOrientationFilter();
  }


  // -----------------------------------------------------
  // ORIENTATION CHECK
  // -----------------------------------------------------

  else if (
    state ==
    ORIENTATION_CHECK
  ) {

    setIssue(
      ISSUE_NONE
    );

    pendingPlungerStart =
        false;

    resetPreOrientationFilter();
  }


  // -----------------------------------------------------
  // HOLDING
  // -----------------------------------------------------

  else if (
    state ==
    HOLDING
  ) {

    pendingPlungerStart =
        false;

    // No vibration after valid PLUNGER DOWN.
    stopVibration();


    holdStartRoll =
        rollDeg;

    holdStartPitch =
        pitchDeg;

    holdStartMs =
        millis();

    holdElapsedMs =
        0;

    resetHoldDriftFilter();

    setIssue(
      ISSUE_NONE
    );

    showGreen();


    Serial.println(
      ">>> HOLD START REFERENCE CAPTURED"
    );

    Serial.print(
      "START ROLL = "
    );

    Serial.println(
      holdStartRoll,
      2
    );

    Serial.print(
      "START PITCH = "
    );

    Serial.println(
      holdStartPitch,
      2
    );

    Serial.print(
      "TARGET HOLD = "
    );

    Serial.print(
      holdTargetMs /
      1000.0f,
      1
    );

    Serial.println(
      " sec"
    );
  }


  // -----------------------------------------------------
  // HOLD COMPLETE
  // -----------------------------------------------------

  else if (
    state ==
    HOLD_COMPLETE
  ) {

    holdElapsedMs =
        holdTargetMs;

    setIssue(
      ISSUE_NONE
    );

    stopVibration();

    showGreen();

    startBuzzer();


    Serial.println();

    Serial.println(
      "=========================="
    );

    Serial.println(
      "       HOLD COMPLETE"
    );

    Serial.println(
      "      RELEASE PLUNGER"
    );

    Serial.println(
      "=========================="
    );
  }


  // -----------------------------------------------------
  // SUCCESS
  // -----------------------------------------------------

  else if (
    state ==
    RESULT_SUCCESS
  ) {

    setIssue(
      ISSUE_NONE
    );

    stopVibration();

    showGreen();


    Serial.println();

    Serial.println(
      "=========================="
    );

    Serial.println(
      "       SESSION PASS"
    );

    Serial.print(
      "Movement warnings: "
    );

    Serial.println(
      holdDriftWarningCount
    );

    Serial.print(
      "Max Roll drift: "
    );

    Serial.print(
      maxHoldDriftRoll,
      2
    );

    Serial.println(
      " deg"
    );

    Serial.print(
      "Max Pitch drift: "
    );

    Serial.print(
      maxHoldDriftPitch,
      2
    );

    Serial.println(
      " deg"
    );

    Serial.println(
      "=========================="
    );
  }


  // -----------------------------------------------------
  // INTERRUPTED
  // -----------------------------------------------------

  else if (
    state ==
    RESULT_INTERRUPTED
  ) {

    stopVibration();

    showRed();


    Serial.println();

    Serial.println(
      "=========================="
    );

    Serial.println(
      "     SESSION INTERRUPTED"
    );

    Serial.print(
      "ISSUE: "
    );

    Serial.println(
      issueName(
        currentIssue
      )
    );

    Serial.println(
      "=========================="
    );
  }
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

    // Plunger pressed before preparation.
    // Visual feedback only.
    if (
      stablePlunger ==
      LOW
    ) {

      showRed();

      setIssue(
        PLUNGER_TOO_EARLY,
        false
      );

      return;
    }


    // No contact.
    if (
      !padContact
      &&
      !penContact
    ) {

      setIssue(
        ISSUE_NONE
      );

      ledsOff();

      return;
    }


    // Partial contact.
    if (
      padContact !=
      penContact
    ) {

      showRed();

      setIssue(
        PARTIAL_CONTACT,
        false
      );

      return;
    }


    // Both contacts confirmed.
    if (
      contactConfirmed
    ) {

      setState(
        ORIENTATION_CHECK
      );

      return;
    }
  }


  // =====================================================
  // ORIENTATION CHECK
  //
  // Before PLUNGER:
  // BAD  -> Red + Vibration
  // GOOD -> Green
  // =====================================================

  else if (
    state ==
    ORIENTATION_CHECK
  ) {

    if (
      !contactConfirmed
    ) {

      pendingPlungerStart =
          false;

      setState(
        READY
      );

      return;
    }


    if (
      plungerDownEvent
    ) {

      pendingPlungerStart =
          true;

      Serial.println(
        ">>> PLUNGER START REQUEST"
      );
    }


    if (
      plungerUpEvent
    ) {

      pendingPlungerStart =
          false;
    }


    if (
      !imuDecisionValid()
    ) {

      return;
    }


    bool good =
        updatePreOrientation();


    // ---------------------------------------------------
    // BAD PRE-INJECTION ORIENTATION
    // ---------------------------------------------------

    if (!good) {

      showRed();

      setIssue(
        BAD_ORIENTATION_PRE,
        true
      );

      if (
        stablePlunger ==
        LOW
      ) {

        pendingPlungerStart =
            false;
      }

      return;
    }


    // ---------------------------------------------------
    // GOOD PRE-INJECTION ORIENTATION
    // ---------------------------------------------------

    setIssue(
      ISSUE_NONE
    );

    showGreen();


    // ---------------------------------------------------
    // VALID PLUNGER START
    // ---------------------------------------------------

    if (
      pendingPlungerStart
    ) {

      if (
        stablePlunger !=
        LOW
      ) {

        pendingPlungerStart =
            false;

        return;
      }


      if (
        !preOrientationRawGood()
      ) {

        return;
      }


      pendingPlungerStart =
          false;

      setState(
        HOLDING
      );

      return;
    }


    // ---------------------------------------------------
    // PLUNGER HELD WITHOUT VALID START EVENT
    // ---------------------------------------------------

    if (
      stablePlunger ==
      LOW
    ) {

      showRed();

      setIssue(
        PLUNGER_TOO_EARLY,
        false
      );

      return;
    }
  }


  // =====================================================
  // HOLDING
  //
  // - Timer continues continuously.
  // - Reference = pose at PLUNGER DOWN.
  // - Movement warning = Red LED only.
  // - No vibration.
  // - Contact changes do not terminate HOLD.
  // =====================================================

  else if (
    state ==
    HOLDING
  ) {

    holdElapsedMs =
        now -
        holdStartMs;


    // ---------------------------------------------------
    // HOLD COMPLETE HAS HIGHEST PRIORITY
    // ---------------------------------------------------

    if (
      holdElapsedMs >=
      holdTargetMs
    ) {

      setState(
        HOLD_COMPLETE
      );

      return;
    }


    // ---------------------------------------------------
    // EARLY PLUNGER RELEASE
    // ---------------------------------------------------

    if (
      plungerUpEvent
    ) {

      setIssue(
        EARLY_RELEASE
      );

      setState(
        RESULT_INTERRUPTED
      );

      return;
    }


    // ---------------------------------------------------
    // HOLD ORIENTATION STABILITY
    // ---------------------------------------------------

    if (
      imuDecisionValid()
    ) {

      updateHoldDrift();


      // Movement warning.
      if (
        holdDriftWarning
      ) {

        showRed();
      }


      // Stable or recovered.
      else {

        if (
          currentIssue ==
          HOLD_MOVEMENT
        ) {

          setIssue(
            ISSUE_NONE
          );
        }

        showGreen();
      }
    }
  }


  // =====================================================
  // HOLD COMPLETE
  // =====================================================

  else if (
    state ==
    HOLD_COMPLETE
  ) {

    showGreen();


    if (
      stablePlunger ==
      HIGH
    ) {

      setState(
        RESULT_SUCCESS
      );

      return;
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
      RESULT_INTERRUPTED
  ) {

    return;
  }
}


// =========================================================
// RESET SESSION
// =========================================================

void resetSession() {

  digitalWrite(
    VIBRATION_PIN,
    LOW
  );

  digitalWrite(
    BUZZER_PIN,
    LOW
  );


  vibrationActive =
      false;

  buzzerActive =
      false;

  pendingPlungerStart =
      false;

  serialWaitingForZero =
      false;


  ledsOff();


  currentIssue =
      ISSUE_NONE;


  holdStartMs =
      0;

  holdElapsedMs =
      0;

  holdStartRoll =
      0.0f;

  holdStartPitch =
      0.0f;


  resetPreOrientationFilter();

  resetHoldDriftFilter();


  imuIgnoreUntilMs =
      0;


  state =
      WAIT_SITE;


  Serial.println();

  Serial.println(
    "=== STATE -> WAIT_SITE ==="
  );

  Serial.print(
    "CURRENT HOLD TARGET = "
  );

  Serial.print(
    holdTargetMs /
    1000.0f,
    1
  );

  Serial.println(
    " sec"
  );

  Serial.println(
    "3 / 6 / 10 = target, S = start, N = reset"
  );
}


// =========================================================
// SET HOLD TARGET
// =========================================================

void setHoldTarget(
  unsigned long targetMs
) {

  if (
    state !=
    WAIT_SITE
  ) {

    Serial.println(
      "Change hold time before session start."
    );

    return;
  }


  holdTargetMs =
      targetMs;


  Serial.print(
    ">>> HOLD TARGET SET: "
  );

  Serial.print(
    holdTargetMs /
    1000.0f,
    1
  );

  Serial.println(
    " sec"
  );
}


// =========================================================
// SERIAL CONTROL
//
// 3  = 3 sec
// 6  = 6 sec
// 10 = 10 sec
// S  = start
// N  = reset
// H  = help
// =========================================================

void handleSerialCommands() {

  while (
    Serial.available()
  ) {

    char c =
        Serial.read();


    if (
      c == '\r' ||
      c == '\n' ||
      c == ' '
    ) {
      continue;
    }


    // ---------------------------------------------------
    // COMPLETE "10"
    // ---------------------------------------------------

    if (
      serialWaitingForZero
    ) {

      if (
        c == '0'
      ) {

        setHoldTarget(
          10000
        );

        serialWaitingForZero =
            false;

        continue;
      }

      serialWaitingForZero =
          false;
    }


    // ---------------------------------------------------
    // START OF "10"
    // ---------------------------------------------------

    if (
      c == '1'
    ) {

      serialWaitingForZero =
          true;

      continue;
    }


    // ---------------------------------------------------
    // 3 SEC
    // ---------------------------------------------------

    if (
      c == '3'
    ) {

      setHoldTarget(
        3000
      );
    }


    // ---------------------------------------------------
    // 6 SEC
    // ---------------------------------------------------

    else if (
      c == '6'
    ) {

      setHoldTarget(
        6000
      );
    }


    // ---------------------------------------------------
    // START
    // ---------------------------------------------------

    else if (
      c == 's' ||
      c == 'S'
    ) {

      if (
        state ==
        WAIT_SITE
      ) {

        Serial.println(
          ">>> SESSION START"
        );

        setState(
          READY
        );
      }
    }


    // ---------------------------------------------------
    // NEW / RESET
    // ---------------------------------------------------

    else if (
      c == 'n' ||
      c == 'N'
    ) {

      Serial.println(
        ">>> NEW SESSION"
      );

      resetSession();
    }


    // ---------------------------------------------------
    // HELP
    // ---------------------------------------------------

    else if (
      c == 'h' ||
      c == 'H'
    ) {

      Serial.println();

      Serial.println(
        "3  = Hold target 3 sec"
      );

      Serial.println(
        "6  = Hold target 6 sec"
      );

      Serial.println(
        "10 = Hold target 10 sec"
      );

      Serial.println(
        "S  = Start session"
      );

      Serial.println(
        "N  = New/reset session"
      );
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


  float preDR =
      angleDiffAbs(
        rollDeg,
        TARGET_ROLL
      );


  float preDP =
      angleDiffAbs(
        pitchDeg,
        TARGET_PITCH
      );


  float holdDR =
      angleDiffAbs(
        rollDeg,
        holdStartRoll
      );


  float holdDP =
      angleDiffAbs(
        pitchDeg,
        holdStartPitch
      );


  Serial.print(
    "STATE="
  );

  Serial.print(
    stateName(state)
  );


  Serial.print(
    " | PAD="
  );

  Serial.print(
    fsrPadValue
  );

  Serial.print(
    padContact
      ? "(ON)"
      : "(OFF)"
  );


  Serial.print(
    " | PEN="
  );

  Serial.print(
    fsrPenValue
  );

  Serial.print(
    penContact
      ? "(ON)"
      : "(OFF)"
  );


  Serial.print(
    " | ROLL="
  );

  Serial.print(
    rollDeg,
    2
  );


  Serial.print(
    " | PITCH="
  );

  Serial.print(
    pitchDeg,
    2
  );


  if (
    state ==
      HOLDING
    ||
    state ==
      HOLD_COMPLETE
    ||
    state ==
      RESULT_SUCCESS
  ) {

    Serial.print(
      " | DRIFT_R="
    );

    Serial.print(
      holdDR,
      2
    );


    Serial.print(
      " | DRIFT_P="
    );

    Serial.print(
      holdDP,
      2
    );


    Serial.print(
      " | STABILITY="
    );

    Serial.print(
      holdDriftWarning
        ? "WARN"
        : "GOOD"
    );
  }


  else {

    Serial.print(
      " | dR="
    );

    Serial.print(
      preDR,
      2
    );


    Serial.print(
      " | dP="
    );

    Serial.print(
      preDP,
      2
    );
  }


  Serial.print(
    " | PLUNGER="
  );

  Serial.print(
    stablePlunger ==
      LOW
      ? "DOWN"
      : "UP"
  );


  Serial.print(
    " | HOLD="
  );

  Serial.print(
    holdElapsedMs /
    1000.0f,
    2
  );

  Serial.print(
    "/"
  );

  Serial.print(
    holdTargetMs /
    1000.0f,
    1
  );

  Serial.print(
    "s"
  );


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


  lastRawPlunger =
      digitalRead(
        PLUNGER_PIN
      );

  stablePlunger =
      lastRawPlunger;


  Wire.begin(
    SDA_PIN,
    SCL_PIN
  );

  Wire.setClock(
    400000
  );


  Serial.println();

  Serial.println(
    "======================================"
  );

  Serial.println(
    "KOKCHI STATE MACHINE TEST"
  );

  Serial.println(
    "======================================"
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

      delay(
        1000
      );
    }
  }


  Serial.println();

  Serial.println(
    "PRE-INJECTION"
  );


  Serial.print(
    "TARGET ROLL  = "
  );

  Serial.println(
    TARGET_ROLL,
    2
  );


  Serial.print(
    "TARGET PITCH = "
  );

  Serial.println(
    TARGET_PITCH,
    2
  );


  Serial.print(
    "ROLL TOL     = +/- "
  );

  Serial.print(
    PRE_ROLL_TOLERANCE,
    1
  );

  Serial.println(
    " deg"
  );


  Serial.print(
    "PITCH TOL    = +/- "
  );

  Serial.print(
    PRE_PITCH_TOLERANCE,
    1
  );

  Serial.println(
    " deg"
  );


  Serial.println();

  Serial.println(
    "DURING HOLD"
  );


  Serial.print(
    "ROLL WARN    = +/- "
  );

  Serial.print(
    HOLD_DRIFT_WARN_ROLL,
    1
  );

  Serial.println(
    " deg"
  );


  Serial.print(
    "PITCH WARN   = +/- "
  );

  Serial.print(
    HOLD_DRIFT_WARN_PITCH,
    1
  );

  Serial.println(
    " deg"
  );


  Serial.print(
    "CLEAR RANGE  = +/- "
  );

  Serial.print(
    HOLD_DRIFT_CLEAR_ROLL,
    1
  );

  Serial.println(
    " deg"
  );


  Serial.print(
    "WARN CONFIRM = "
  );

  Serial.print(
    HOLD_DRIFT_WARN_CONFIRM_MS
  );

  Serial.println(
    " ms"
  );


  Serial.print(
    "CLEAR CONFIRM = "
  );

  Serial.print(
    HOLD_DRIFT_CLEAR_CONFIRM_MS
  );

  Serial.println(
    " ms"
  );


  Serial.println();

  Serial.println(
    "HOLD TARGET OPTIONS"
  );

  Serial.println(
    "3 sec / 6 sec / 10 sec"
  );


  resetSession();
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


  // 2. CONTACT
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


  // 3. PLUNGER
  updatePlunger();


  // 4. OUTPUT TIMERS
  updateOutputs();


  // 5. SERIAL CONTROL
  handleSerialCommands();


  // 6. STATE MACHINE
  updateStateMachine();


  // 7. DEBUG STATUS
  printStatus();
}