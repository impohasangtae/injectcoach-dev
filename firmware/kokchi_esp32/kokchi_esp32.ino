#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <math.h>

// =====================================================
// KOKCHI - Embedded State Machine + Web HMI v4.0
// ESP32 is the single source of truth.
// =====================================================

// ------------------------- Pin map -------------------------
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

// ---------------------- Calibrated values ------------------
const int FSR_PAD_THRESHOLD = 2000;
const int FSR_PEN_THRESHOLD = 1500;

const float TARGET_ROLL = -7.56f;
const float TARGET_PITCH = 52.83f;
const float PRE_ROLL_TOLERANCE = 7.0f;
const float PRE_PITCH_TOLERANCE = 7.0f;
const unsigned long PRE_BAD_CONFIRM_MS = 350;

const float HOLD_DRIFT_WARN_ROLL = 6.0f;
const float HOLD_DRIFT_WARN_PITCH = 6.0f;
const unsigned long HOLD_DRIFT_WARN_CONFIRM_MS = 250;
const float HOLD_DRIFT_CLEAR_ROLL = 4.5f;
const float HOLD_DRIFT_CLEAR_PITCH = 4.5f;
const unsigned long HOLD_DRIFT_CLEAR_CONFIRM_MS = 200;

const unsigned long VIBRATION_MS = 70;
const unsigned long IMU_SETTLE_MS = 250;
const unsigned long BUZZER_MS = 350;
const unsigned long DEBOUNCE_MS = 30;
const unsigned long PRINT_INTERVAL_MS = 200;

unsigned long holdTargetMs = 3000;

enum SystemState {
  WAIT_SITE,
  READY,
  ORIENTATION_CHECK,
  HOLDING,
  HOLD_COMPLETE,
  RESULT_SUCCESS,
  RESULT_INTERRUPTED
};

enum IssueCode {
  ISSUE_NONE,
  PARTIAL_CONTACT,
  BAD_ORIENTATION_PRE,
  PLUNGER_TOO_EARLY,
  HOLD_MOVEMENT,
  EARLY_RELEASE
};

SystemState state = WAIT_SITE;
IssueCode currentIssue = ISSUE_NONE;

float rollDeg = 0.0f;
float pitchDeg = 0.0f;
bool imuReadOK = false;
bool imuInitOK = false;

int fsrPadValue = 0;
int fsrPenValue = 0;
bool padContact = false;
bool penContact = false;
bool contactConfirmed = false;

bool preBadTiming = false;
bool preBadConfirmed = false;
unsigned long preBadStartMs = 0;

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

bool lastRawPlunger = HIGH;
bool stablePlunger = HIGH;
bool plungerDownEvent = false;
bool plungerUpEvent = false;
bool pendingPlungerStart = false;
unsigned long debounceStartMs = 0;

unsigned long holdStartMs = 0;
unsigned long holdElapsedMs = 0;

bool vibrationActive = false;
bool buzzerActive = false;
bool greenLedOn = false;
bool redLedOn = false;
unsigned long vibrationEndMs = 0;
unsigned long buzzerEndMs = 0;
unsigned long imuIgnoreUntilMs = 0;

bool serialWaitingForZero = false;
unsigned long lastPrintMs = 0;

float angleDiffAbs(float a, float b) {
  float d = fmodf(a - b + 540.0f, 360.0f) - 180.0f;
  return fabsf(d);
}

bool initIMU() {
  Wire.beginTransmission(IMU_ADDR);
  Wire.write(0x6B);
  Wire.write(0x00);
  if (Wire.endTransmission() != 0) return false;
  delay(100);

  Wire.beginTransmission(IMU_ADDR);
  Wire.write(0x75);
  if (Wire.endTransmission(false) != 0) return false;
  Wire.requestFrom(IMU_ADDR, (uint8_t)1);
  if (!Wire.available()) return false;

  uint8_t whoAmI = Wire.read();
  Serial.print("WHO_AM_I = 0x");
  if (whoAmI < 0x10) Serial.print("0");
  Serial.println(whoAmI, HEX);
  return whoAmI != 0x00 && whoAmI != 0xFF;
}

bool readIMU(float &roll, float &pitch) {
  Wire.beginTransmission(IMU_ADDR);
  Wire.write(0x3B);
  if (Wire.endTransmission(false) != 0) return false;
  Wire.requestFrom(IMU_ADDR, (uint8_t)6);
  if (Wire.available() < 6) return false;

  int16_t rawAx = ((int16_t)Wire.read() << 8) | Wire.read();
  int16_t rawAy = ((int16_t)Wire.read() << 8) | Wire.read();
  int16_t rawAz = ((int16_t)Wire.read() << 8) | Wire.read();

  float ax = rawAx / 16384.0f;
  float ay = rawAy / 16384.0f;
  float az = rawAz / 16384.0f;

  roll = atan2f(ay, az) * 180.0f / PI;
  pitch = atan2f(-ax, sqrtf(ay * ay + az * az)) * 180.0f / PI;
  return true;
}

bool imuDecisionValid() {
  if (!imuReadOK) return false;
  return (long)(millis() - imuIgnoreUntilMs) >= 0;
}

bool preOrientationRawGood() {
  return angleDiffAbs(rollDeg, TARGET_ROLL) <= PRE_ROLL_TOLERANCE &&
         angleDiffAbs(pitchDeg, TARGET_PITCH) <= PRE_PITCH_TOLERANCE;
}

bool updatePreOrientation() {
  unsigned long now = millis();
  if (preOrientationRawGood()) {
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
  if (now - preBadStartMs >= PRE_BAD_CONFIRM_MS) {
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

void updateHoldDrift() {
  unsigned long now = millis();
  float dRoll = angleDiffAbs(rollDeg, holdStartRoll);
  float dPitch = angleDiffAbs(pitchDeg, holdStartPitch);

  if (dRoll > maxHoldDriftRoll) maxHoldDriftRoll = dRoll;
  if (dPitch > maxHoldDriftPitch) maxHoldDriftPitch = dPitch;

  if (!holdDriftWarning) {
    bool beyondWarn = dRoll > HOLD_DRIFT_WARN_ROLL || dPitch > HOLD_DRIFT_WARN_PITCH;
    if (beyondWarn) {
      if (!holdDriftBadTiming) {
        holdDriftBadTiming = true;
        holdDriftBadStartMs = now;
      } else if (now - holdDriftBadStartMs >= HOLD_DRIFT_WARN_CONFIRM_MS) {
        holdDriftWarning = true;
        holdDriftBadTiming = false;
        holdDriftClearTiming = false;
        holdDriftWarningCount++;
        currentIssue = HOLD_MOVEMENT;
        Serial.println(">>> HOLD MOVEMENT WARNING");
      }
    } else {
      holdDriftBadTiming = false;
      holdDriftBadStartMs = 0;
    }
  } else {
    bool insideClear = dRoll <= HOLD_DRIFT_CLEAR_ROLL &&
                       dPitch <= HOLD_DRIFT_CLEAR_PITCH;
    if (insideClear) {
      if (!holdDriftClearTiming) {
        holdDriftClearTiming = true;
        holdDriftClearStartMs = now;
      } else if (now - holdDriftClearStartMs >= HOLD_DRIFT_CLEAR_CONFIRM_MS) {
        holdDriftWarning = false;
        holdDriftClearTiming = false;
        holdDriftBadTiming = false;
        currentIssue = ISSUE_NONE;
        Serial.println("<<< HOLD STABILITY RESTORED");
      }
    } else {
      holdDriftClearTiming = false;
      holdDriftClearStartMs = 0;
    }
  }
}

void ledsOff() {
  digitalWrite(GREEN_LED_PIN, LOW);
  digitalWrite(RED_LED_PIN, LOW);
  greenLedOn = false;
  redLedOn = false;
}

void showGreen() {
  digitalWrite(GREEN_LED_PIN, HIGH);
  digitalWrite(RED_LED_PIN, LOW);
  greenLedOn = true;
  redLedOn = false;
}

void showRed() {
  digitalWrite(GREEN_LED_PIN, LOW);
  digitalWrite(RED_LED_PIN, HIGH);
  greenLedOn = false;
  redLedOn = true;
}

void startVibration() {
  if (vibrationActive) return;
  unsigned long now = millis();
  digitalWrite(VIBRATION_PIN, HIGH);
  vibrationActive = true;
  vibrationEndMs = now + VIBRATION_MS;
  imuIgnoreUntilMs = now + VIBRATION_MS + IMU_SETTLE_MS;
  Serial.println(">>> VIBRATION WARNING");
}

void stopVibration() {
  digitalWrite(VIBRATION_PIN, LOW);
  vibrationActive = false;
}

void startBuzzer() {
  if (buzzerActive) return;
  digitalWrite(BUZZER_PIN, HIGH);
  buzzerActive = true;
  buzzerEndMs = millis() + BUZZER_MS;
  Serial.println(">>> HOLD COMPLETE BEEP");
}

void updateOutputs() {
  unsigned long now = millis();
  if (vibrationActive && (long)(now - vibrationEndMs) >= 0) {
    digitalWrite(VIBRATION_PIN, LOW);
    vibrationActive = false;
  }
  if (buzzerActive && (long)(now - buzzerEndMs) >= 0) {
    digitalWrite(BUZZER_PIN, LOW);
    buzzerActive = false;
  }
}

void setIssue(IssueCode newIssue, bool vibrateOnNewIssue = false) {
  if (newIssue == currentIssue) return;
  currentIssue = newIssue;
  if (newIssue != ISSUE_NONE && vibrateOnNewIssue) startVibration();
}

void updatePlunger() {
  plungerDownEvent = false;
  plungerUpEvent = false;
  unsigned long now = millis();
  bool raw = digitalRead(PLUNGER_PIN);

  if (raw != lastRawPlunger) {
    lastRawPlunger = raw;
    debounceStartMs = now;
  }
  if (now - debounceStartMs >= DEBOUNCE_MS && raw != stablePlunger) {
    stablePlunger = raw;
    if (stablePlunger == LOW) {
      plungerDownEvent = true;
      Serial.println(">>> PLUNGER DOWN");
    } else {
      plungerUpEvent = true;
      Serial.println("<<< PLUNGER RELEASE");
    }
  }
}

const char* stateName(SystemState value) {
  switch (value) {
    case WAIT_SITE: return "WAIT_SITE";
    case READY: return "READY";
    case ORIENTATION_CHECK: return "ORIENTATION_CHECK";
    case HOLDING: return "HOLDING";
    case HOLD_COMPLETE: return "HOLD_COMPLETE";
    case RESULT_SUCCESS: return "RESULT_SUCCESS";
    case RESULT_INTERRUPTED: return "RESULT_INTERRUPTED";
    default: return "UNKNOWN";
  }
}

const char* issueName(IssueCode value) {
  switch (value) {
    case ISSUE_NONE: return "NONE";
    case PARTIAL_CONTACT: return "PARTIAL_CONTACT";
    case BAD_ORIENTATION_PRE: return "BAD_ORIENTATION_PRE";
    case PLUNGER_TOO_EARLY: return "PLUNGER_TOO_EARLY";
    case HOLD_MOVEMENT: return "HOLD_MOVEMENT";
    case EARLY_RELEASE: return "EARLY_RELEASE";
    default: return "UNKNOWN";
  }
}

void setState(SystemState newState) {
  if (state == newState) return;
  state = newState;
  Serial.print("=== STATE -> ");
  Serial.print(stateName(state));
  Serial.println(" ===");

  if (state == READY) {
    setIssue(ISSUE_NONE);
    pendingPlungerStart = false;
    ledsOff();
    resetPreOrientationFilter();
  } else if (state == ORIENTATION_CHECK) {
    setIssue(ISSUE_NONE);
    pendingPlungerStart = false;
    resetPreOrientationFilter();
  } else if (state == HOLDING) {
    pendingPlungerStart = false;
    stopVibration();
    holdStartRoll = rollDeg;
    holdStartPitch = pitchDeg;
    holdStartMs = millis();
    holdElapsedMs = 0;
    resetHoldDriftFilter();
    setIssue(ISSUE_NONE);
    showGreen();
    Serial.println(">>> HOLD START REFERENCE CAPTURED");
  } else if (state == HOLD_COMPLETE) {
    holdElapsedMs = holdTargetMs;
    setIssue(ISSUE_NONE);
    stopVibration();
    showGreen();
    startBuzzer();
    Serial.println(">>> HOLD COMPLETE - RELEASE PLUNGER");
  } else if (state == RESULT_SUCCESS) {
    setIssue(ISSUE_NONE);
    stopVibration();
    showGreen();
    Serial.println(">>> SESSION PASS");
  } else if (state == RESULT_INTERRUPTED) {
    stopVibration();
    showRed();
    Serial.print(">>> SESSION INTERRUPTED: ");
    Serial.println(issueName(currentIssue));
  }
}

void updateStateMachine() {
  unsigned long now = millis();

  if (state == WAIT_SITE) {
    ledsOff();
    return;
  }

  if (state == READY) {
    if (stablePlunger == LOW) {
      showRed();
      setIssue(PLUNGER_TOO_EARLY);
      return;
    }
    if (!padContact && !penContact) {
      setIssue(ISSUE_NONE);
      ledsOff();
      return;
    }
    if (padContact != penContact) {
      showRed();
      setIssue(PARTIAL_CONTACT);
      return;
    }
    if (contactConfirmed) {
      setState(ORIENTATION_CHECK);
      return;
    }
  } else if (state == ORIENTATION_CHECK) {
    if (!contactConfirmed) {
      pendingPlungerStart = false;
      setState(READY);
      return;
    }
    if (plungerDownEvent) {
      pendingPlungerStart = true;
      Serial.println(">>> PLUNGER START REQUEST");
    }
    if (plungerUpEvent) pendingPlungerStart = false;
    if (!imuDecisionValid()) return;

    bool good = updatePreOrientation();
    if (!good) {
      showRed();
      setIssue(BAD_ORIENTATION_PRE, true);
      if (stablePlunger == LOW) pendingPlungerStart = false;
      return;
    }

    setIssue(ISSUE_NONE);
    showGreen();
    if (pendingPlungerStart) {
      if (stablePlunger != LOW) {
        pendingPlungerStart = false;
        return;
      }
      if (!preOrientationRawGood()) return;
      pendingPlungerStart = false;
      setState(HOLDING);
      return;
    }
    if (stablePlunger == LOW) {
      showRed();
      setIssue(PLUNGER_TOO_EARLY);
      return;
    }
  } else if (state == HOLDING) {
    holdElapsedMs = now - holdStartMs;
    if (holdElapsedMs >= holdTargetMs) {
      setState(HOLD_COMPLETE);
      return;
    }
    if (plungerUpEvent) {
      setIssue(EARLY_RELEASE);
      setState(RESULT_INTERRUPTED);
      return;
    }
    if (imuDecisionValid()) {
      updateHoldDrift();
      if (holdDriftWarning) {
        showRed();
      } else {
        if (currentIssue == HOLD_MOVEMENT) setIssue(ISSUE_NONE);
        showGreen();
      }
    }
  } else if (state == HOLD_COMPLETE) {
    showGreen();
    if (stablePlunger == HIGH) {
      setState(RESULT_SUCCESS);
      return;
    }
  }
}

void resetSession() {
  digitalWrite(VIBRATION_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  vibrationActive = false;
  buzzerActive = false;
  pendingPlungerStart = false;
  serialWaitingForZero = false;
  ledsOff();
  currentIssue = ISSUE_NONE;
  holdStartMs = 0;
  holdElapsedMs = 0;
  holdStartRoll = 0.0f;
  holdStartPitch = 0.0f;
  resetPreOrientationFilter();
  resetHoldDriftFilter();
  imuIgnoreUntilMs = 0;
  state = WAIT_SITE;
  Serial.println("=== STATE -> WAIT_SITE ===");
}

bool setHoldTarget(unsigned long targetMs) {
  if (state != WAIT_SITE) return false;
  if (targetMs != 3000 && targetMs != 6000 && targetMs != 10000) return false;
  holdTargetMs = targetMs;
  Serial.print(">>> HOLD TARGET SET: ");
  Serial.print(holdTargetMs / 1000.0f, 1);
  Serial.println(" sec");
  return true;
}

void handleSerialCommands() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\r' || c == '\n' || c == ' ') continue;
    if (serialWaitingForZero) {
      serialWaitingForZero = false;
      if (c == '0') {
        setHoldTarget(10000);
        continue;
      }
    }
    if (c == '1') {
      serialWaitingForZero = true;
    } else if (c == '3') {
      setHoldTarget(3000);
    } else if (c == '6') {
      setHoldTarget(6000);
    } else if ((c == 's' || c == 'S') && state == WAIT_SITE) {
      setState(READY);
    } else if (c == 'n' || c == 'N') {
      resetSession();
    }
  }
}

void printStatus() {
  unsigned long now = millis();
  if (now - lastPrintMs < PRINT_INTERVAL_MS) return;
  lastPrintMs = now;

  Serial.print("STATE=");
  Serial.print(stateName(state));
  Serial.print(" | PAD=");
  Serial.print(fsrPadValue);
  Serial.print(padContact ? "(ON)" : "(OFF)");
  Serial.print(" | PEN=");
  Serial.print(fsrPenValue);
  Serial.print(penContact ? "(ON)" : "(OFF)");
  Serial.print(" | ROLL=");
  Serial.print(rollDeg, 2);
  Serial.print(" | PITCH=");
  Serial.print(pitchDeg, 2);
  Serial.print(" | PLUNGER=");
  Serial.print(stablePlunger == LOW ? "DOWN" : "UP");
  Serial.print(" | HOLD=");
  Serial.print(holdElapsedMs / 1000.0f, 2);
  Serial.print("/");
  Serial.print(holdTargetMs / 1000.0f, 1);
  Serial.print("s | ISSUE=");
  Serial.println(issueName(currentIssue));
}

// =====================================================
// KOKCHI - Integrated Web HMI Prototype v3.2
// Rotation + Injection Session + Result / History
// =====================================================

const char* AP_SSID = "InjectCoach-Test";
const char* AP_PASSWORD = "12345678";

WebServer server(80);

const char PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="ko">

<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">

  <title>KOKCHI 자가주사 수행 보조 시스템</title>

  <style>
    * {
      box-sizing: border-box;
    }

    :root {
      --bg: #f4f6f8;
      --card: #ffffff;
      --text: #20242a;
      --muted: #747d87;
      --line: #dce1e6;
      --soft: #f5f7f9;
      --dark: #20242a;
      --recent: #d7dce2;
      --history: #edf0f3;
      --warning-bg: #f3ece7;
      --warning-line: #9a6747;
      --ok-bg: #edf3ef;
      --ok-line: #617b69;
    }

    body {
      margin: 0;
      font-family: Arial, "Noto Sans KR", sans-serif;
      background: var(--bg);
      color: var(--text);
    }

    .container {
      width: min(920px, 92%);
      margin: 0 auto;
      padding: 26px 0 60px;
    }

    .header {
      margin-bottom: 18px;
    }

    .brand {
      font-size: 34px;
      font-weight: 800;
      letter-spacing: 2px;
    }

    .subtitle {
      margin-top: 5px;
      color: var(--muted);
      font-size: 14px;
    }

    .profile-row {
      margin-top: 12px;
      display: flex;
      align-items: center;
      gap: 8px;
      flex-wrap: wrap;
    }

    .profile-label {
      font-size: 12px;
      color: #858d97;
      font-weight: 700;
    }

    .profile-chip {
      background: #e9edf2;
      padding: 7px 11px;
      border-radius: 999px;
      font-size: 13px;
      font-weight: 700;
    }

    /* =========================
       Stepper
       ========================= */

    .stepper {
      background: white;
      border-radius: 18px;
      padding: 18px 16px;
      margin-bottom: 16px;
      box-shadow: 0 5px 20px rgba(0,0,0,0.05);
    }

    .stepper-row {
      display: grid;
      grid-template-columns: 1fr 42px 1fr 42px 1fr;
      align-items: center;
      gap: 4px;
    }

    .step-node {
      text-align: center;
      min-width: 0;
    }

    .step-circle {
      width: 30px;
      height: 30px;
      border-radius: 50%;
      border: 2px solid #cfd5db;
      background: white;
      color: #8d959e;
      display: flex;
      align-items: center;
      justify-content: center;
      margin: 0 auto 7px;
      font-size: 12px;
      font-weight: 800;
    }

    .step-title {
      font-size: 12px;
      color: #8a929b;
      font-weight: 700;
      white-space: nowrap;
    }

    .step-line {
      height: 2px;
      background: #dce1e6;
      margin-top: -19px;
    }

    .step-node.active .step-circle {
      background: var(--dark);
      color: white;
      border-color: var(--dark);
    }

    .step-node.active .step-title {
      color: var(--dark);
      font-weight: 800;
    }

    .step-node.done .step-circle {
      background: #e8ecef;
      color: var(--dark);
      border-color: #aeb6bf;
    }

    .step-line.done {
      background: #aeb6bf;
    }

    /* =========================
       Common
       ========================= */

    .card {
      background: var(--card);
      border-radius: 18px;
      padding: 22px;
      margin-bottom: 16px;
      box-shadow: 0 5px 20px rgba(0,0,0,0.06);
    }

    .section-number {
      font-size: 12px;
      color: #9299a2;
      font-weight: 800;
      margin-bottom: 6px;
    }

    .card-title {
      font-size: 19px;
      font-weight: 800;
      margin-bottom: 7px;
    }

    .card-desc {
      font-size: 13px;
      color: var(--muted);
      line-height: 1.6;
      margin-bottom: 18px;
    }

    .step-panel {
      display: none;
    }

    .step-panel.visible {
      display: block;
    }

    .state-box {
      padding: 15px 16px;
      background: var(--soft);
      border-radius: 14px;
      margin-bottom: 16px;
    }

    .state-label {
      font-size: 12px;
      color: #7d858f;
      margin-bottom: 5px;
      font-weight: 700;
    }

    .state-value {
      font-size: 22px;
      font-weight: 800;
    }

    button {
      border: 1px solid #d9dde3;
      background: white;
      color: #252a30;
      border-radius: 12px;
      padding: 12px 14px;
      font-size: 14px;
      cursor: pointer;
      transition: 0.15s;
    }

    button:active {
      transform: scale(0.98);
    }

    button.selected {
      background: var(--dark);
      border-color: var(--dark);
      color: white;
    }

    .action-button {
      width: 100%;
      margin-top: 16px;
      background: var(--dark);
      color: white;
      border-color: var(--dark);
      font-weight: 700;
      padding: 14px;
    }

    .action-button.secondary {
      background: white;
      color: var(--dark);
      border-color: #cfd5db;
    }

    .action-button:disabled {
      background: #c8cdd3;
      border-color: #c8cdd3;
      color: white;
      cursor: default;
    }

    /* =========================
       STEP 1
       ========================= */

    .profile-rule-grid {
      display: grid;
      grid-template-columns: repeat(3, 1fr);
      gap: 8px;
      margin-bottom: 14px;
    }

    .profile-rule-item {
      padding: 11px 10px;
      border: 1px solid var(--line);
      border-radius: 12px;
      background: #fafbfc;
    }

    .profile-rule-label {
      display: block;
      font-size: 9px;
      color: #8a929b;
      font-weight: 800;
      margin-bottom: 4px;
    }

    .profile-rule-value {
      display: block;
      font-size: 12px;
      color: var(--text);
      font-weight: 800;
      line-height: 1.35;
    }

    .rotation-principle {
      padding: 13px 14px;
      border-radius: 12px;
      background: #f7f8f9;
      margin-bottom: 18px;
      font-size: 12px;
      color: #69727c;
      line-height: 1.6;
    }

    .rotation-principle strong {
      color: var(--text);
    }

    .relative-zone-note {
      margin: -2px 0 15px;
      font-size: 10px;
      color: #9098a1;
      line-height: 1.55;
    }

    .region-buttons {
      display: grid;
      grid-template-columns: repeat(2, 1fr);
      gap: 9px;
      margin-bottom: 20px;
    }

    .map-title-row {
      display: flex;
      align-items: flex-end;
      justify-content: space-between;
      gap: 12px;
      margin-bottom: 10px;
    }

    .map-region-name {
      font-size: 16px;
      font-weight: 800;
    }

    .map-guide {
      font-size: 11px;
      color: #969da6;
    }

    .rotation-map {
      display: grid;
      grid-template-columns: repeat(2, 1fr);
      gap: 9px;
      margin-bottom: 14px;
    }

    .site-cell {
      position: relative;
      min-height: 82px;
      border: 1px solid var(--line);
      border-radius: 14px;
      background: white;
      display: flex;
      flex-direction: column;
      align-items: center;
      justify-content: center;
      gap: 5px;
      font-weight: 800;
      cursor: pointer;
      transition: 0.15s;
      overflow: hidden;
    }

    .site-cell.recent {
      background: var(--recent);
    }

    .site-cell.history {
      background: var(--history);
    }

    .site-cell.selected {
      border: 3px solid var(--dark);
      background: white;
    }

    .site-number {
      font-size: 18px;
    }

    .cell-badges {
      min-height: 17px;
      display: flex;
      justify-content: center;
      gap: 4px;
      flex-wrap: wrap;
    }

    .cell-badge {
      font-size: 8px;
      line-height: 1;
      padding: 4px 6px;
      border-radius: 999px;
      background: rgba(255,255,255,0.82);
      border: 1px solid rgba(70,75,82,0.18);
      color: #59616a;
      letter-spacing: 0.4px;
      font-weight: 800;
    }

    .cell-badge.selected-badge {
      background: var(--dark);
      color: white;
      border-color: var(--dark);
    }

    .legend {
      display: flex;
      flex-wrap: wrap;
      gap: 12px;
      margin-bottom: 18px;
    }

    .legend-item {
      display: flex;
      align-items: center;
      gap: 6px;
      font-size: 11px;
      color: #737b85;
    }

    .legend-dot {
      width: 13px;
      height: 13px;
      border-radius: 4px;
      border: 1px solid #d5dae0;
      background: white;
    }

    .legend-dot.recent-dot {
      background: var(--recent);
    }

    .legend-dot.history-dot {
      background: var(--history);
    }

    .legend-dot.selected-dot {
      background: white;
      border: 2px solid var(--dark);
    }

    .rotation-check {
      border-radius: 14px;
      padding: 16px;
      background: var(--soft);
      border-left: 4px solid transparent;
    }

    .rotation-check.warning {
      background: var(--warning-bg);
      border-left-color: var(--warning-line);
    }

    .rotation-check.ok {
      background: var(--ok-bg);
      border-left-color: var(--ok-line);
    }

    .check-title {
      font-weight: 800;
      margin-bottom: 6px;
    }

    .check-message {
      font-size: 13px;
      color: #666f79;
      line-height: 1.55;
    }

    .current-site {
      font-size: 22px;
      font-weight: 800;
      margin: 16px 0 5px;
    }

    .current-note {
      color: #7a828c;
      font-size: 12px;
      line-height: 1.5;
    }

    .hold-setting {
      margin-top: 18px;
      padding: 16px;
      border: 1px solid var(--line);
      border-radius: 14px;
      background: #fbfcfd;
    }

    .hold-setting-title {
      font-size: 13px;
      font-weight: 800;
      margin-bottom: 5px;
    }

    .hold-setting-desc {
      font-size: 11px;
      color: #858d97;
      line-height: 1.5;
      margin-bottom: 11px;
    }

    .hold-options {
      display: grid;
      grid-template-columns: repeat(3, 1fr);
      gap: 8px;
    }

    .hold-option {
      font-weight: 800;
      padding: 12px 8px;
    }

    .device-status {
      display: inline-flex;
      align-items: center;
      gap: 6px;
      margin-top: 10px;
      padding: 6px 10px;
      border-radius: 999px;
      background: #e9edf2;
      color: #66707a;
      font-size: 11px;
      font-weight: 800;
    }

    .device-status.connected {
      background: var(--ok-bg);
      color: #486253;
    }

    .device-status.disconnected {
      background: var(--warning-bg);
      color: #81543a;
    }

    /* =========================
       STEP 2
       ========================= */

    .session-site {
      padding: 14px 15px;
      border-radius: 14px;
      background: var(--soft);
      margin-bottom: 14px;
    }

    .session-site-label {
      font-size: 11px;
      color: #818a94;
      font-weight: 700;
      margin-bottom: 4px;
    }

    .session-site-value {
      font-size: 18px;
      font-weight: 800;
    }

    .sensor-grid {
      display: grid;
      grid-template-columns: repeat(2, 1fr);
      gap: 10px;
    }

    .sensor-card {
      border: 1px solid var(--line);
      border-radius: 14px;
      padding: 14px;
      min-height: 96px;
    }

    .guidance-box {
      padding: 16px;
      border-radius: 14px;
      background: var(--soft);
      border-left: 4px solid #98a1aa;
      margin-bottom: 14px;
    }

    .guidance-box.good {
      background: var(--ok-bg);
      border-left-color: var(--ok-line);
    }

    .guidance-box.warning,
    .guidance-box.error {
      background: var(--warning-bg);
      border-left-color: var(--warning-line);
    }

    .guidance-title {
      font-size: 18px;
      font-weight: 800;
      margin-bottom: 5px;
    }

    .guidance-message {
      color: #626c76;
      font-size: 13px;
      line-height: 1.55;
    }

    .hold-progress-wrap {
      margin: 14px 0;
      padding: 15px;
      border: 1px solid var(--line);
      border-radius: 14px;
    }

    .hold-progress-head {
      display: flex;
      align-items: baseline;
      justify-content: space-between;
      gap: 10px;
      margin-bottom: 9px;
    }

    .hold-progress-label {
      font-size: 12px;
      color: #7d858f;
      font-weight: 800;
    }

    .hold-progress-time {
      font-size: 20px;
      font-weight: 800;
    }

    .hold-progress-track {
      height: 10px;
      border-radius: 999px;
      background: #e7eaee;
      overflow: hidden;
    }

    .hold-progress-bar {
      width: 0%;
      height: 100%;
      background: var(--dark);
      border-radius: inherit;
      transition: width 0.15s linear;
    }

    .technical-details {
      margin-top: 14px;
      border: 1px solid var(--line);
      border-radius: 14px;
      overflow: hidden;
    }

    .technical-details summary {
      cursor: pointer;
      padding: 13px 14px;
      font-size: 12px;
      font-weight: 800;
      color: #68727c;
    }

    .debug-grid {
      display: grid;
      grid-template-columns: repeat(2, 1fr);
      gap: 1px;
      background: var(--line);
      border-top: 1px solid var(--line);
    }

    .debug-item {
      background: white;
      padding: 11px 13px;
    }

    .debug-label {
      display: block;
      color: #8a929b;
      font-size: 10px;
      margin-bottom: 4px;
    }

    .debug-value {
      font-size: 12px;
      font-weight: 800;
      word-break: break-all;
    }

    .sensor-label {
      font-size: 11px;
      color: #858d97;
      font-weight: 800;
      margin-bottom: 8px;
    }

    .sensor-value {
      font-size: 17px;
      font-weight: 800;
      margin-bottom: 4px;
    }

    .sensor-sub {
      font-size: 10px;
      color: #9299a2;
      line-height: 1.4;
    }

    .demo-note {
      margin-top: 14px;
      padding: 12px 13px;
      border-radius: 12px;
      background: #f7f7f8;
      color: #858d97;
      font-size: 11px;
      line-height: 1.6;
    }

    .button-row {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 8px;
      margin-top: 14px;
    }

    .button-row .action-button {
      margin-top: 0;
    }

    /* =========================
       STEP 3
       ========================= */

    .result-banner {
      padding: 18px;
      border-radius: 15px;
      background: var(--soft);
      margin-bottom: 14px;
    }

    .result-banner.success {
      background: var(--ok-bg);
      border-left: 4px solid var(--ok-line);
    }

    .result-banner.interrupted {
      background: var(--warning-bg);
      border-left: 4px solid var(--warning-line);
    }

    .result-kicker {
      font-size: 11px;
      color: #818a94;
      font-weight: 800;
      margin-bottom: 5px;
    }

    .result-title {
      font-size: 23px;
      font-weight: 800;
    }

    .result-table {
      display: flex;
      flex-direction: column;
      border: 1px solid var(--line);
      border-radius: 14px;
      overflow: hidden;
      margin-bottom: 14px;
    }

    .result-row {
      display: grid;
      grid-template-columns: 105px 1fr;
      gap: 10px;
      padding: 12px 14px;
      border-bottom: 1px solid #edf0f2;
      font-size: 13px;
    }

    .result-row:last-child {
      border-bottom: none;
    }

    .result-label {
      color: #7b848e;
      font-weight: 700;
    }

    .result-value {
      font-weight: 800;
    }

    .confirmation {
      border: 1px solid var(--line);
      border-radius: 14px;
      padding: 16px;
      margin-top: 14px;
    }

    .confirm-site {
      font-size: 19px;
      font-weight: 800;
      margin: 8px 0 14px;
    }

    .previous-action {
      width: 100%;
      margin: 0 0 18px;
      background: white;
      color: var(--dark);
      border-color: #cfd5db;
      font-weight: 700;
    }

    .confirm-buttons {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 10px;
    }

    .confirm-button {
      background: var(--dark);
      color: white;
      border-color: var(--dark);
      font-weight: 800;
    }

    .correct-button {
      background: white;
      color: var(--dark);
      border-color: #cfd5db;
      font-weight: 700;
    }

    .recorded-box {
      display: none;
      padding: 16px;
      border-radius: 14px;
      background: var(--ok-bg);
      border-left: 4px solid var(--ok-line);
      margin-top: 14px;
    }

    .recorded-box.visible {
      display: block;
    }

    /* =========================
       History
       ========================= */

    .history-empty {
      font-size: 13px;
      color: #858d97;
      padding: 13px;
      background: var(--soft);
      border-radius: 12px;
    }

    .history-list {
      display: flex;
      flex-direction: column;
      gap: 8px;
    }

    .history-item {
      display: grid;
      grid-template-columns: 32px 1fr auto;
      align-items: center;
      gap: 10px;
      padding: 11px 12px;
      background: #f6f7f8;
      border-radius: 12px;
    }

    .history-index {
      font-size: 11px;
      font-weight: 800;
      color: #89919b;
    }

    .history-site {
      font-size: 13px;
      font-weight: 700;
    }

    .history-meta {
      margin-top: 3px;
      font-size: 10px;
      color: #9198a0;
      line-height: 1.35;
    }

    .history-status {
      font-size: 9px;
      color: #727a83;
      font-weight: 800;
      text-align: right;
    }

    .prototype-note {
      margin-top: 10px;
      font-size: 11px;
      color: #9aa1a9;
      line-height: 1.6;
    }

    @media (max-width: 600px) {
      .container {
        width: 92%;
        padding-top: 22px;
      }

      .brand {
        font-size: 29px;
      }

      .card {
        padding: 19px;
      }

      .stepper {
        padding: 16px 10px;
      }

      .stepper-row {
        grid-template-columns: 1fr 26px 1fr 26px 1fr;
      }

      .step-title {
        font-size: 11px;
      }

      .sensor-grid {
        grid-template-columns: 1fr 1fr;
      }

      .profile-rule-grid {
        grid-template-columns: 1fr 1fr 1fr;
        gap: 6px;
      }

      .profile-rule-item {
        padding: 10px 7px;
      }

      .profile-rule-value {
        font-size: 11px;
      }

      .result-row {
        grid-template-columns: 86px 1fr;
      }
    }
  </style>
</head>


<body>

<div class="container">

  <!-- Header -->
  <div class="header">
    <div class="brand">KOKCHI</div>
    <div class="subtitle">자가주사 수행 보조 시스템 · Functional Embedded Prototype</div>

    <div class="profile-row">
      <span class="profile-label">현재 사용 모드</span>
      <span class="profile-chip">데모 프로필</span>
    </div>

    <div class="device-status" id="deviceConnection">기기 연결 확인 중</div>
  </div>


  <!-- Stepper -->
  <div class="stepper">
    <div class="stepper-row">

      <div class="step-node active" id="stepNode1">
        <div class="step-circle" id="stepCircle1">1</div>
        <div class="step-title">위치 선택</div>
      </div>

      <div class="step-line" id="stepLine1"></div>

      <div class="step-node" id="stepNode2">
        <div class="step-circle" id="stepCircle2">2</div>
        <div class="step-title">주사 수행</div>
      </div>

      <div class="step-line" id="stepLine2"></div>

      <div class="step-node" id="stepNode3">
        <div class="step-circle" id="stepCircle3">3</div>
        <div class="step-title">결과 / 기록</div>
      </div>

    </div>
  </div>


  <!-- =====================================================
       STEP 1
       ===================================================== -->

  <div class="step-panel visible" id="step1Panel">

    <div class="card">
      <div class="section-number">STEP 01</div>
      <div class="card-title">주사 위치 로테이션</div>
      <div class="card-desc">
        데모 프로필에서 사용할 주사 부위와 상대적 세부 위치를 선택합니다.
        선택한 위치는 최근 확인 이력과 비교되며, 세션 완료 후 사용자가 확인한 경우에만 기록됩니다.
      </div>

      <div class="profile-rule-grid">
        <div class="profile-rule-item">
          <span class="profile-rule-label">허용 부위</span>
          <span class="profile-rule-value" id="allowedRegionSummary">4개</span>
        </div>

        <div class="profile-rule-item">
          <span class="profile-rule-label">최근 비교</span>
          <span class="profile-rule-value" id="recentWindowSummary">3회</span>
        </div>

        <div class="profile-rule-item">
          <span class="profile-rule-label">기록 기준</span>
          <span class="profile-rule-value">세션 확인 후</span>
        </div>
      </div>

      <div class="rotation-principle">
        <strong>Rotation Assistance 기준</strong><br>
        같은 주사 부위라도 세부 위치를 구분해 이력을 관리합니다.
        최근 사용한 동일 세부 위치는 경고하며, 시스템이 특정 위치를 자동 처방하지는 않습니다.
      </div>

      <div class="map-title-row">
        <div class="map-region-name">주사 부위 선택</div>
        <div class="map-guide">Allowed regions · Demo Profile</div>
      </div>

      <div class="region-buttons">
        <button data-region-key="abdomen" onclick="selectRegion(this, 'abdomen')">복부</button>
        <button data-region-key="leftThigh" onclick="selectRegion(this, 'leftThigh')">왼쪽 허벅지</button>
        <button data-region-key="rightThigh" onclick="selectRegion(this, 'rightThigh')">오른쪽 허벅지</button>
        <button data-region-key="upperArm" onclick="selectRegion(this, 'upperArm')">위팔</button>
      </div>

      <div class="map-title-row">
        <div class="map-region-name" id="mapRegionName">부위를 먼저 선택해주세요</div>
        <div class="map-guide">상대적 세부 위치</div>
      </div>

      <div class="rotation-map" id="rotationMap">
        <div class="site-cell" data-index="1" onclick="selectSubRegion(1)">
          <div class="site-number">1</div>
          <div class="cell-badges"></div>
        </div>

        <div class="site-cell" data-index="2" onclick="selectSubRegion(2)">
          <div class="site-number">2</div>
          <div class="cell-badges"></div>
        </div>

        <div class="site-cell" data-index="3" onclick="selectSubRegion(3)">
          <div class="site-number">3</div>
          <div class="cell-badges"></div>
        </div>

        <div class="site-cell" data-index="4" onclick="selectSubRegion(4)">
          <div class="site-number">4</div>
          <div class="cell-badges"></div>
        </div>

        <div class="site-cell" data-index="5" onclick="selectSubRegion(5)">
          <div class="site-number">5</div>
          <div class="cell-badges"></div>
        </div>

        <div class="site-cell" data-index="6" onclick="selectSubRegion(6)">
          <div class="site-number">6</div>
          <div class="cell-badges"></div>
        </div>
      </div>

      <div class="relative-zone-note">
        ※ 1–6은 데모 프로필의 상대적 위치 구역입니다.
        실제 피부의 cm 단위 안전거리나 자동 위치 판정을 의미하지 않습니다.
      </div>

      <div class="legend">
        <div class="legend-item">
          <span class="legend-dot"></span>
          사용 가능
        </div>

        <div class="legend-item">
          <span class="legend-dot recent-dot"></span>
          가장 최근
        </div>

        <div class="legend-item">
          <span class="legend-dot history-dot"></span>
          최근 이력
        </div>

        <div class="legend-item">
          <span class="legend-dot selected-dot"></span>
          현재 선택
        </div>
      </div>

      <div class="rotation-check" id="rotationCheck">
        <div class="check-title">최근 사용 위치 확인</div>
        <div class="check-message" id="rotationMessage">
          위치를 선택하면 최근 확인된 이력과 비교합니다.
        </div>
      </div>

      <div class="current-site" id="currentSite">선택된 위치 없음</div>

      <div class="current-note">
        현재 선택은 아직 사용 이력에 저장되지 않습니다.
      </div>

      <div class="hold-setting">
        <div class="hold-setting-title">동작 유지시간</div>
        <div class="hold-setting-desc">
          현재 데모 프로필에서 사용할 유지시간을 선택합니다.
        </div>
        <div class="hold-options">
          <button class="hold-option selected" data-hold-sec="3" onclick="selectHoldTime(this, 3)">3초</button>
          <button class="hold-option" data-hold-sec="6" onclick="selectHoldTime(this, 6)">6초</button>
          <button class="hold-option" data-hold-sec="10" onclick="selectHoldTime(this, 10)">10초</button>
        </div>
      </div>

      <button class="action-button" id="startSessionButton" onclick="startInjectionSession()" disabled>
        이 위치로 시작
      </button>
    </div>


    <div class="card">
      <div class="section-number">RECENT HISTORY</div>
      <div class="card-title">최근 주사 위치 기록</div>
      <div class="card-desc">
        확인된 세션만 표시됩니다. 최근 기록은 다음 위치 선택 시 Rotation Check에 사용됩니다.
      </div>

      <div id="historyContainerStep1">
        <div class="history-empty">아직 확인된 기록이 없습니다.</div>
      </div>
    </div>

  </div>


  <!-- =====================================================
       STEP 2
       ===================================================== -->

  <div class="step-panel" id="step2Panel">

    <div class="card">
      <div class="section-number">STEP 02</div>
      <div class="card-title">주사 수행</div>
      <div class="card-desc">
        센서 입력과 수행 단계는 ESP32가 판단합니다. 화면에는 실제 상태와 진행 결과가 그대로 표시됩니다.
      </div>

      <div class="session-site">
        <div class="session-site-label">현재 선택 위치</div>
        <div class="session-site-value" id="sessionSite">-</div>
      </div>

      <div class="state-box">
        <div class="state-label">현재 단계</div>
        <div class="state-value" id="sessionState">연결 확인 중</div>
      </div>

      <div class="guidance-box" id="guidanceBox">
        <div class="guidance-title" id="guidanceTitle">세션을 준비하고 있습니다</div>
        <div class="guidance-message" id="guidanceMessage">ESP32 상태를 불러오는 중입니다.</div>
      </div>

      <div class="sensor-grid">

        <div class="sensor-card">
          <div class="sensor-label">PAD 접촉</div>
          <div class="sensor-value" id="padValue">대기</div>
          <div class="sensor-sub" id="padSub">FSR-PAD · GPIO32</div>
        </div>

        <div class="sensor-card">
          <div class="sensor-label">PEN 접촉</div>
          <div class="sensor-value" id="penValue">대기</div>
          <div class="sensor-sub" id="penSub">FSR-PEN · GPIO34</div>
        </div>

        <div class="sensor-card">
          <div class="sensor-label">자세 안정성</div>
          <div class="sensor-value" id="orientationValue">대기</div>
          <div class="sensor-sub" id="orientationSub">MPU6050 · Roll / Pitch</div>
        </div>

        <div class="sensor-card">
          <div class="sensor-label">플런저</div>
          <div class="sensor-value" id="plungerValue">대기</div>
          <div class="sensor-sub">Tact Button · GPIO27</div>
        </div>

        <div class="sensor-card">
          <div class="sensor-label">Rotation</div>
          <div class="sensor-value" id="rotationValue">확인 완료</div>
          <div class="sensor-sub">최근 확인 이력 비교</div>
        </div>

        <div class="sensor-card">
          <div class="sensor-label">물리 피드백</div>
          <div class="sensor-value" id="feedbackValue">대기</div>
          <div class="sensor-sub">LED · 진동 · 부저</div>
        </div>

      </div>

      <div class="hold-progress-wrap">
        <div class="hold-progress-head">
          <span class="hold-progress-label">ESP32 유지시간</span>
          <span class="hold-progress-time" id="holdValue">0.00 / 3.00초</span>
        </div>
        <div class="hold-progress-track">
          <div class="hold-progress-bar" id="holdProgressBar"></div>
        </div>
      </div>

      <details class="technical-details">
        <summary>센서 및 판단 상세값</summary>
        <div class="debug-grid">
          <div class="debug-item"><span class="debug-label">Roll / Pitch</span><span class="debug-value" id="debugPose">-</span></div>
          <div class="debug-item"><span class="debug-label">기준 자세 차이</span><span class="debug-value" id="debugPreDelta">-</span></div>
          <div class="debug-item"><span class="debug-label">Hold 시작 자세</span><span class="debug-value" id="debugHoldStart">-</span></div>
          <div class="debug-item"><span class="debug-label">Hold 자세 변화</span><span class="debug-value" id="debugHoldDrift">-</span></div>
          <div class="debug-item"><span class="debug-label">State</span><span class="debug-value" id="debugState">-</span></div>
          <div class="debug-item"><span class="debug-label">Issue</span><span class="debug-value" id="debugIssue">-</span></div>
        </div>
      </details>

      <button class="action-button secondary" onclick="backToSiteSelection()">
        세션 취소 후 위치 다시 선택
      </button>

    </div>

  </div>


  <!-- =====================================================
       STEP 3
       ===================================================== -->

  <div class="step-panel" id="step3Panel">

    <div class="card">
      <div class="section-number">STEP 03</div>
      <div class="card-title">결과 및 기록</div>

      <div class="result-banner" id="resultBanner">
        <div class="result-kicker">SESSION RESULT</div>
        <div class="result-title" id="resultTitle">세션 완료</div>
      </div>

      <div class="result-table">

        <div class="result-row">
          <div class="result-label">위치</div>
          <div class="result-value" id="resultSite">-</div>
        </div>

        <div class="result-row">
          <div class="result-label">Rotation</div>
          <div class="result-value" id="resultRotation">-</div>
        </div>

        <div class="result-row">
          <div class="result-label">접촉</div>
          <div class="result-value" id="resultContact">-</div>
        </div>

        <div class="result-row">
          <div class="result-label">동작 안정성</div>
          <div class="result-value" id="resultStability">-</div>
        </div>

        <div class="result-row">
          <div class="result-label">Hold</div>
          <div class="result-value" id="resultHold">-</div>
        </div>

        <div class="result-row">
          <div class="result-label">최대 자세 변화</div>
          <div class="result-value" id="resultMaxDrift">-</div>
        </div>

      </div>

      <div class="confirmation" id="confirmation">
        <div class="check-title">실제 수행 위치 확인</div>

        <div class="check-message">
          실제 수행한 위치가 아래 선택과 일치합니까?
          확인한 경우에만 주사 위치 기록에 저장됩니다.
        </div>

        <div class="confirm-site" id="confirmSite">-</div>

        <button
          class="previous-action"
          id="backToInjectionButton"
          onclick="backToInjectionSession()"
          style="display:none;">
          ← 주사 수행 상태 보기
        </button>

        <div class="confirm-buttons">
          <button class="correct-button" onclick="correctSite()">
            위치 수정
          </button>

          <button class="confirm-button" onclick="confirmSite()">
            이 위치로 기록
          </button>
        </div>
      </div>

      <div class="recorded-box" id="recordedBox">
        <div class="check-title">기록 완료</div>
        <div class="check-message">
          확인된 위치가 주사 위치 기록에 저장되었습니다.
        </div>
      </div>

      <button class="action-button" id="newSessionButton" onclick="startNewSession()" style="display:none;">
        새 세션 시작
      </button>
    </div>


    <div class="card">
      <div class="section-number">CONFIRMED HISTORY</div>
      <div class="card-title">주사 위치 기록</div>
      <div class="card-desc">
        사용자가 세션 완료 후 확인한 위치만 저장합니다.
      </div>

      <div id="historyContainerStep3">
        <div class="history-empty">아직 확인된 기록이 없습니다.</div>
      </div>

      <div class="prototype-note">
        ※ 확인된 이력은 현재 휴대폰 브라우저에 저장됩니다.
        같은 브라우저에서는 새로고침하거나 다시 접속해도 유지되며,
        다른 기기 또는 브라우저 데이터 삭제 시에는 공유·복원되지 않습니다.
      </div>
    </div>

  </div>

</div>


<script>

// =====================================================
// Configuration
// =====================================================

const DEMO_PROFILE = {
  name: "데모 프로필",
  allowedRegions: [
    "abdomen",
    "leftThigh",
    "rightThigh",
    "upperArm"
  ],
  recentWindow: 3,
  maxHistory: 10
};

const REGION_CONFIG = {
  abdomen: {
    name: "복부",
    code: "A"
  },
  leftThigh: {
    name: "왼쪽 허벅지",
    code: "LT"
  },
  rightThigh: {
    name: "오른쪽 허벅지",
    code: "RT"
  },
  upperArm: {
    name: "위팔",
    code: "UA"
  }
};

const RECENT_WINDOW = DEMO_PROFILE.recentWindow;
const MAX_HISTORY = DEMO_PROFILE.maxHistory;
const HISTORY_STORAGE_KEY = "kokchi_history_v1";


// =====================================================
// State variables
// =====================================================

let currentStep = 1;

let selectedRegionKey = "";
let selectedSubRegion = null;
let currentSessionSite = "";

let sessionHadRotationWarning = false;
let selectedHoldSec = 3;
let sessionActive = false;
let resultPresented = false;
let isCorrectingSite = false;
let lastStatus = null;
let statusRequestInFlight = false;

// Confirmed sessions only
let historyList = [];


function loadHistory() {

  try {

    const storedHistory =
      localStorage.getItem(HISTORY_STORAGE_KEY);

    if (!storedHistory) {
      historyList = [];
      return;
    }

    const parsedHistory = JSON.parse(storedHistory);

    if (!Array.isArray(parsedHistory)) {
      historyList = [];
      return;
    }

    historyList = parsedHistory
      .filter((item) =>
        item &&
        typeof item.site === "string" &&
        item.site.length > 0
      )
      .slice(0, MAX_HISTORY);

  } catch (error) {

    historyList = [];
    console.warn("History load failed", error);
  }
}


function saveHistory() {

  try {

    localStorage.setItem(
      HISTORY_STORAGE_KEY,
      JSON.stringify(historyList.slice(0, MAX_HISTORY))
    );

  } catch (error) {

    console.warn("History save failed", error);
  }
}


function formatHistoryTime(value) {

  if (!value) {
    return "";
  }

  const date = new Date(value);

  if (Number.isNaN(date.getTime())) {
    return "";
  }

  return date.toLocaleString("ko-KR", {
    year: "numeric",
    month: "2-digit",
    day: "2-digit",
    hour: "2-digit",
    minute: "2-digit"
  });
}


// =====================================================
// Device / Protocol Profile
// =====================================================

function applyProfile() {

  document
    .querySelectorAll(".region-buttons button")
    .forEach(button => {

      const key =
        button.dataset.regionKey;

      const allowed =
        DEMO_PROFILE.allowedRegions.includes(key);

      button.style.display =
        allowed ? "block" : "none";
    });

  document.getElementById("allowedRegionSummary").innerText =
    DEMO_PROFILE.allowedRegions.length + "개";

  document.getElementById("recentWindowSummary").innerText =
    DEMO_PROFILE.recentWindow + "회";
}


function selectHoldTime(button, seconds) {

  selectedHoldSec = seconds;

  document
    .querySelectorAll(".hold-option")
    .forEach(item => item.classList.remove("selected"));

  button.classList.add("selected");
}


async function postForm(url, values = {}) {

  const body = new URLSearchParams(values);

  const response = await fetch(url, {
    method: "POST",
    headers: {
      "Content-Type": "application/x-www-form-urlencoded"
    },
    body: body.toString(),
    cache: "no-store"
  });

  const payload = await response.json();

  if (!response.ok) {
    throw new Error(payload.message || "요청을 처리하지 못했습니다.");
  }

  return payload;
}


// =====================================================
// Step navigation
// =====================================================

function showStep(step) {

  currentStep = step;

  document.getElementById("step1Panel")
    .classList.toggle("visible", step === 1);

  document.getElementById("step2Panel")
    .classList.toggle("visible", step === 2);

  document.getElementById("step3Panel")
    .classList.toggle("visible", step === 3);

  updateStepper();
  window.scrollTo({ top: 0, behavior: "smooth" });
}


function updateStepper() {

  for (let i = 1; i <= 3; i++) {

    const node =
      document.getElementById("stepNode" + i);

    const circle =
      document.getElementById("stepCircle" + i);

    node.classList.remove("active", "done");

    if (i < currentStep) {
      node.classList.add("done");
      circle.innerText = "✓";
    } else if (i === currentStep) {
      node.classList.add("active");
      circle.innerText = String(i);
    } else {
      circle.innerText = String(i);
    }
  }

  document.getElementById("stepLine1")
    .classList.toggle("done", currentStep >= 2);

  document.getElementById("stepLine2")
    .classList.toggle("done", currentStep >= 3);
}


// =====================================================
// Region / site selection
// =====================================================

function selectRegion(button, regionKey) {

  document
    .querySelectorAll(".region-buttons button")
    .forEach(b => b.classList.remove("selected"));

  button.classList.add("selected");

  selectedRegionKey = regionKey;
  selectedSubRegion = null;
  currentSessionSite = "";

  const region =
    REGION_CONFIG[selectedRegionKey];

  document.getElementById("mapRegionName").innerText =
    region.name + " · 세부 위치";

  document.getElementById("currentSite").innerText =
    region.name + " / 세부 위치 선택";

  document.getElementById("startSessionButton").disabled =
    true;

  clearRotationMessage();

  renderRotationMap();
}


function selectSubRegion(index) {

  if (selectedRegionKey === "") {
    alert("먼저 주사 부위를 선택해주세요.");
    return;
  }

  selectedSubRegion = index;

  const region =
    REGION_CONFIG[selectedRegionKey];

  currentSessionSite =
    region.name + " / " + region.code + index;

  document.getElementById("currentSite").innerText =
    currentSessionSite;

  document.getElementById("startSessionButton").disabled =
    false;

  runRotationCheck();
  renderRotationMap();
}


function clearRotationMessage() {

  const box =
    document.getElementById("rotationCheck");

  box.classList.remove("warning", "ok");

  document.getElementById("rotationMessage").innerText =
    "세부 위치를 선택하면 최근 확인된 이력과 비교합니다.";
}


// =====================================================
// Rotation logic
// =====================================================

function isCurrentSiteRecent() {

  const recentHistory =
    historyList.slice(0, RECENT_WINDOW);

  return recentHistory.some(
    item => item.site === currentSessionSite
  );
}


function runRotationCheck() {

  const box =
    document.getElementById("rotationCheck");

  const message =
    document.getElementById("rotationMessage");

  const usedRecently =
    isCurrentSiteRecent();

  sessionHadRotationWarning =
    usedRecently;

  box.classList.remove("warning", "ok");

  if (usedRecently) {

    box.classList.add("warning");

    message.innerHTML =
      "<strong>최근 사용한 동일 세부 위치입니다.</strong><br>" +
      "현재 선택은 최근 " +
      RECENT_WINDOW +
      "회의 확인 이력에 포함됩니다. " +
      "같은 주사 부위 안에서도 다른 세부 위치를 고려할 수 있습니다.";

  } else {

    box.classList.add("ok");

    message.innerHTML =
      "<strong>최근 동일 세부 위치 기록이 없습니다.</strong><br>" +
      "현재 선택은 최근 " +
      RECENT_WINDOW +
      "회의 확인 이력과 동일하지 않습니다.";
  }
}


// =====================================================
// Rotation map rendering
// =====================================================

function renderRotationMap() {

  document
    .querySelectorAll(".site-cell")
    .forEach(cell => {

      cell.classList.remove(
        "selected",
        "recent",
        "history"
      );

      const badgeContainer =
        cell.querySelector(".cell-badges");

      badgeContainer.innerHTML = "";

      if (selectedRegionKey === "") {
        return;
      }

      const index =
        parseInt(cell.dataset.index);

      const region =
        REGION_CONFIG[selectedRegionKey];

      const siteName =
        region.name + " / " + region.code + index;

      const recentHistory =
        historyList.slice(0, RECENT_WINDOW);

      const historyOrder =
        recentHistory.findIndex(
          item => item.site === siteName
        );

      if (historyOrder === 0) {

        cell.classList.add("recent");

        addCellBadge(
          badgeContainer,
          "RECENT"
        );

      } else if (historyOrder > 0) {

        cell.classList.add("history");

        addCellBadge(
          badgeContainer,
          "HISTORY"
        );
      }

      if (selectedSubRegion === index) {

        cell.classList.add("selected");

        addCellBadge(
          badgeContainer,
          "SELECTED",
          true
        );
      }
    });
}


function addCellBadge(container, text, selected = false) {

  const badge =
    document.createElement("span");

  badge.className =
    "cell-badge" +
    (selected ? " selected-badge" : "");

  badge.innerText = text;

  container.appendChild(badge);
}


// =====================================================
// STEP 1 -> STEP 2
// =====================================================

async function startInjectionSession() {

  if (currentSessionSite === "") {
    return;
  }

  if (isCorrectingSite) {

    sessionHadRotationWarning =
      isCurrentSiteRecent();

    document.getElementById("resultSite").innerText =
      currentSessionSite;

    document.getElementById("resultRotation").innerText =
      sessionHadRotationWarning
        ? "최근 위치 경고 확인"
        : "최근 동일 위치 없음";

    document.getElementById("confirmSite").innerText =
      currentSessionSite;

    document.getElementById("confirmation").style.display =
      "block";

    document.getElementById("startSessionButton").innerText =
      "이 위치로 시작";

    isCorrectingSite = false;
    showStep(3);
    return;
  }

  sessionHadRotationWarning =
    isCurrentSiteRecent();

  const startButton =
    document.getElementById("startSessionButton");

  startButton.disabled = true;
  startButton.innerText = "세션을 시작하는 중...";

  try {

    const status = await postForm(
      "/api/session/start",
      { holdMs: String(selectedHoldSec * 1000) }
    );

    sessionActive = true;
    resultPresented = false;
    lastStatus = status;

    document.getElementById("sessionSite").innerText =
      currentSessionSite;

    document.getElementById("rotationValue").innerText =
      sessionHadRotationWarning
        ? "최근 위치 경고"
        : "최근 중복 없음";

    showStep(2);
    renderDeviceStatus(status);

  } catch (error) {
    alert(error.message);
  } finally {
    startButton.disabled = currentSessionSite === "";
    startButton.innerText = "이 위치로 시작";
  }
}


async function backToSiteSelection() {

  if (sessionActive) {
    try {
      await postForm("/api/session/reset");
    } catch (error) {
      alert(error.message);
      return;
    }
  }

  sessionActive = false;
  resultPresented = false;
  isCorrectingSite = false;
  showStep(1);
}


// =====================================================
// DEVICE RESULT -> STEP 3
// =====================================================

function presentResult(status) {

  if (currentSessionSite === "" || resultPresented) {
    return;
  }

  resultPresented = true;

  document.getElementById("resultSite").innerText =
    currentSessionSite;

  document.getElementById("resultRotation").innerText =
    sessionHadRotationWarning
      ? "최근 위치 경고 확인"
      : "최근 동일 위치 없음";

  document.getElementById("confirmSite").innerText =
    currentSessionSite;

  const success =
    status.state === "RESULT_SUCCESS";

  const resultBanner =
    document.getElementById("resultBanner");

  resultBanner.classList.remove("success", "interrupted");
  resultBanner.classList.add(success ? "success" : "interrupted");

  document.getElementById("resultTitle").innerText =
    success ? "세션 완료" : "동작이 중단되었습니다";

  document.getElementById("resultContact").innerText =
    success
      ? "시작 전 PAD / PEN 접촉 확인"
      : "시작 전 접촉 확인 · 유지 중 중단";

  document.getElementById("resultStability").innerText =
    success
      ? "움직임 경고 " + status.movementWarningCount + "회"
      : issueText(status.issue);

  document.getElementById("resultHold").innerText =
    formatSeconds(status.holdElapsedMs) + " / " +
    formatSeconds(status.holdTargetMs) + "초";

  document.getElementById("resultMaxDrift").innerText =
    "Roll " + formatDegree(status.maxHoldDriftRoll) +
    " · Pitch " + formatDegree(status.maxHoldDriftPitch);

  document.getElementById("confirmation").style.display =
    success ? "block" : "none";

  document.getElementById("backToInjectionButton").style.display =
    "none";

  document.getElementById("recordedBox")
    .classList.remove("visible");

  document.getElementById("newSessionButton").style.display =
    success ? "none" : "block";

  showStep(3);
}


// =====================================================
// STEP 3 -> STEP 2
// =====================================================

function backToInjectionSession() {

  if (sessionActive) showStep(2);
}


// =====================================================
// Confirm / Correct
// =====================================================

function confirmSite() {

  if (currentSessionSite === "") {
    return;
  }

  const newRecord = {
    site: currentSessionSite,
    regionKey: selectedRegionKey,
    subRegion: selectedSubRegion,
    result: "COMPLETE",
    rotationWarning: sessionHadRotationWarning,
    confirmedAt: new Date().toISOString()
  };

  historyList.unshift(newRecord);

  if (historyList.length > MAX_HISTORY) {
    historyList.pop();
  }

  saveHistory();
  renderHistory();

  document.getElementById("confirmation").style.display =
    "none";

  document.getElementById("backToInjectionButton").style.display =
    "none";

  document.getElementById("recordedBox")
    .classList.add("visible");

  document.getElementById("newSessionButton").style.display =
    "block";
}


function correctSite() {

  document.getElementById("confirmation").style.display =
    "none";

  document.getElementById("recordedBox")
    .classList.remove("visible");

  document.getElementById("newSessionButton").style.display =
    "none";

  isCorrectingSite = true;

  document.getElementById("startSessionButton").innerText =
    "수정한 위치 적용";

  showStep(1);

  document.getElementById("rotationMessage").innerHTML =
    "<strong>위치 수정 중입니다.</strong><br>" +
    "실제 수행한 위치에 맞게 주사 부위와 세부 위치를 다시 선택해주세요.";

  renderRotationMap();
}


// =====================================================
// New session
// =====================================================

async function startNewSession() {

  try {
    await postForm("/api/session/reset");
  } catch (error) {
    alert(error.message);
    return;
  }

  selectedRegionKey = "";
  selectedSubRegion = null;
  currentSessionSite = "";
  sessionHadRotationWarning = false;
  sessionActive = false;
  resultPresented = false;
  isCorrectingSite = false;
  lastStatus = null;

  document
    .querySelectorAll(".region-buttons button")
    .forEach(b => b.classList.remove("selected"));

  document.getElementById("mapRegionName").innerText =
    "부위를 먼저 선택해주세요";

  document.getElementById("currentSite").innerText =
    "선택된 위치 없음";

  document.getElementById("startSessionButton").disabled =
    true;

  document.getElementById("startSessionButton").innerText =
    "이 위치로 시작";

  clearRotationMessage();
  renderRotationMap();

  document.getElementById("confirmation").style.display =
    "block";

  document.getElementById("backToInjectionButton").style.display =
    "block";

  document.getElementById("recordedBox")
    .classList.remove("visible");

  document.getElementById("newSessionButton").style.display =
    "none";

  showStep(1);
}


// =====================================================
// History
// =====================================================

function renderHistory() {

  renderHistoryInto(
    "historyContainerStep1"
  );

  renderHistoryInto(
    "historyContainerStep3"
  );

  renderRotationMap();
}


function renderHistoryInto(containerId) {

  const container =
    document.getElementById(containerId);

  if (historyList.length === 0) {

    container.innerHTML =
      '<div class="history-empty">' +
      '아직 확인된 기록이 없습니다.' +
      '</div>';

    return;
  }

  let html =
    '<div class="history-list">';

  historyList.forEach((item, index) => {

    const confirmedTime =
      formatHistoryTime(item.confirmedAt);

    html +=
      '<div class="history-item">' +

        '<div class="history-index">' +
          String(index + 1).padStart(2, "0") +
        '</div>' +

        '<div>' +
          '<div class="history-site">' +
            item.site +
          '</div>' +

          '<div class="history-meta">' +
            '데모 프로필 · 완료' +
            (confirmedTime
              ? ' · ' + confirmedTime
              : '') +
            (item.rotationWarning
              ? ' · 최근 위치 경고 발생'
              : '') +
          '</div>' +
        '</div>' +

        '<div class="history-status">' +
          (index === 0
            ? "가장 최근"
            : "확인 기록") +
        '</div>' +

      '</div>';
  });

  html += '</div>';

  container.innerHTML = html;
}


// =====================================================
// ESP32 status polling / HMI mapping
// =====================================================

function formatSeconds(milliseconds) {
  return (Number(milliseconds || 0) / 1000).toFixed(2);
}


function formatDegree(value) {
  return Number(value || 0).toFixed(2) + "°";
}


function issueText(issue) {

  const labels = {
    NONE: "경고 없음",
    PARTIAL_CONTACT: "접촉 조건을 모두 확인해주세요",
    BAD_ORIENTATION_PRE: "시작 자세 교정이 필요합니다",
    PLUNGER_TOO_EARLY: "플런저 입력 순서를 확인해주세요",
    HOLD_MOVEMENT: "유지 중 움직임이 감지되었습니다",
    EARLY_RELEASE: "목표시간 전에 플런저가 해제되었습니다"
  };

  return labels[issue] || issue;
}


function setConnectionState(connected) {

  const element =
    document.getElementById("deviceConnection");

  element.classList.remove("connected", "disconnected");
  element.classList.add(connected ? "connected" : "disconnected");
  element.innerText = connected ? "ESP32 연결됨" : "ESP32 연결 끊김";
}


function setGuidance(style, title, message) {

  const box =
    document.getElementById("guidanceBox");

  box.classList.remove("good", "warning", "error");

  if (style) box.classList.add(style);

  document.getElementById("guidanceTitle").innerText = title;
  document.getElementById("guidanceMessage").innerText = message;
}


function renderGuidance(status) {

  if (status.state === "READY") {

    if (status.issue === "PLUNGER_TOO_EARLY") {
      setGuidance(
        "warning",
        "플런저를 먼저 놓아주세요",
        "접촉과 시작 자세를 확인하기 전에 플런저가 눌려 있습니다."
      );
    } else if (status.issue === "PARTIAL_CONTACT") {
      setGuidance(
        "warning",
        "접촉 상태를 확인해주세요",
        "PAD와 PEN 접촉이 모두 확인되어야 다음 단계로 진행됩니다."
      );
    } else {
      setGuidance(
        "",
        "접촉 준비",
        "주사기를 선택한 위치에 준비해주세요."
      );
    }

  } else if (status.state === "ORIENTATION_CHECK") {

    if (!status.imuReadOK) {
      setGuidance(
        "warning",
        "자세 센서를 확인하고 있습니다",
        "MPU6050 값을 읽을 수 없습니다. 센서 연결을 확인해주세요."
      );
    } else if (status.issue === "BAD_ORIENTATION_PRE") {
      setGuidance(
        "warning",
        "시작 자세 교정이 필요합니다",
        "주사기 자세를 기준 위치에 맞춰주세요. ESP32가 진동 피드백을 제공합니다."
      );
    } else if (status.issue === "PLUNGER_TOO_EARLY") {
      setGuidance(
        "warning",
        "플런저 입력 순서를 확인해주세요",
        "플런저를 놓은 뒤 시작 자세가 확인될 때 다시 눌러주세요."
      );
    } else {
      setGuidance(
        "good",
        "시작 자세가 확인되었습니다",
        "Green LED를 확인하고 플런저를 눌러주세요."
      );
    }

  } else if (status.state === "HOLDING") {

    if (status.issue === "HOLD_MOVEMENT") {
      setGuidance(
        "warning",
        "유지 중 움직임이 감지되었습니다",
        "주입 시작 자세에 가깝게 되돌려주세요. 유지시간은 계속 증가합니다."
      );
    } else {
      setGuidance(
        "good",
        "안정적으로 유지 중입니다",
        "플런저를 누른 상태로 설정된 유지시간까지 자세를 유지해주세요."
      );
    }

  } else if (status.state === "HOLD_COMPLETE") {

    setGuidance(
      "good",
      "유지시간을 충족했습니다",
      "완료 부저를 확인하고 플런저를 놓아주세요."
    );

  } else if (status.state === "RESULT_SUCCESS") {

    setGuidance(
      "good",
      "세션이 완료되었습니다",
      "실제 수행 위치를 확인한 뒤 기록해주세요."
    );

  } else if (status.state === "RESULT_INTERRUPTED") {

    setGuidance(
      "error",
      "동작이 중단되었습니다",
      issueText(status.issue)
    );

  } else {

    setGuidance(
      "",
      "위치 선택 대기",
      "주사 위치와 동작 유지시간을 선택해주세요."
    );
  }
}


function renderDeviceStatus(status) {

  lastStatus = status;

  const stateLabels = {
    WAIT_SITE: "위치 선택 대기",
    READY: "접촉 준비",
    ORIENTATION_CHECK: "시작 자세 확인",
    HOLDING: "유지 중",
    HOLD_COMPLETE: "유지 완료 · 해제 대기",
    RESULT_SUCCESS: "세션 완료",
    RESULT_INTERRUPTED: "세션 중단"
  };

  document.getElementById("sessionState").innerText =
    stateLabels[status.state] || status.state;

  document.getElementById("padValue").innerText =
    status.padContact ? "접촉" : "대기";

  document.getElementById("padSub").innerText =
    "ADC " + status.padValue + " · GPIO32";

  document.getElementById("penValue").innerText =
    status.penContact ? "접촉" : "대기";

  document.getElementById("penSub").innerText =
    "ADC " + status.penValue + " · GPIO34";

  let orientationLabel = "대기";

  if (!status.imuReadOK) {
    orientationLabel = "센서 확인";
  } else if (status.state === "ORIENTATION_CHECK") {
    orientationLabel = status.issue === "BAD_ORIENTATION_PRE"
      ? "교정 필요"
      : "정상";
  } else if (
    status.state === "HOLDING" ||
    status.state === "HOLD_COMPLETE" ||
    status.state === "RESULT_SUCCESS"
  ) {
    orientationLabel = status.holdStability === "WARNING"
      ? "움직임 감지"
      : "안정";
  }

  document.getElementById("orientationValue").innerText =
    orientationLabel;

  document.getElementById("orientationSub").innerText =
    "Roll " + formatDegree(status.roll) +
    " · Pitch " + formatDegree(status.pitch);

  document.getElementById("plungerValue").innerText =
    status.plungerDown ? "누름" : "놓음";

  const feedback = [];
  if (status.greenLedOn) feedback.push("Green");
  if (status.redLedOn) feedback.push("Red");
  if (status.vibrationActive) feedback.push("진동");
  if (status.buzzerActive) feedback.push("부저");

  document.getElementById("feedbackValue").innerText =
    feedback.length ? feedback.join(" + ") : "OFF";

  const elapsed = Number(status.holdElapsedMs || 0);
  const target = Math.max(Number(status.holdTargetMs || 1), 1);
  const percent = Math.min(100, Math.max(0, elapsed / target * 100));

  document.getElementById("holdValue").innerText =
    formatSeconds(elapsed) + " / " + formatSeconds(target) + "초";

  document.getElementById("holdProgressBar").style.width =
    percent.toFixed(1) + "%";

  document.getElementById("debugPose").innerText =
    formatDegree(status.roll) + " / " + formatDegree(status.pitch);

  document.getElementById("debugPreDelta").innerText =
    "Roll " + formatDegree(status.preDeltaRoll) +
    " / Pitch " + formatDegree(status.preDeltaPitch);

  document.getElementById("debugHoldStart").innerText =
    "Roll " + formatDegree(status.holdStartRoll) +
    " / Pitch " + formatDegree(status.holdStartPitch);

  document.getElementById("debugHoldDrift").innerText =
    "Roll " + formatDegree(status.holdDriftRoll) +
    " / Pitch " + formatDegree(status.holdDriftPitch);

  document.getElementById("debugState").innerText =
    status.state;

  document.getElementById("debugIssue").innerText =
    status.issue;

  renderGuidance(status);

  if (
    sessionActive &&
    (status.state === "RESULT_SUCCESS" ||
     status.state === "RESULT_INTERRUPTED")
  ) {
    presentResult(status);
  }

  if (sessionActive && status.state === "WAIT_SITE") {
    sessionActive = false;
    resultPresented = false;
    showStep(1);
  }
}


async function pollDeviceStatus() {

  if (statusRequestInFlight) return;
  statusRequestInFlight = true;

  try {

    const response = await fetch("/api/status", { cache: "no-store" });

    if (!response.ok) {
      throw new Error("status error");
    }

    const status = await response.json();

    setConnectionState(true);

    if (sessionActive || currentStep === 2) {
      renderDeviceStatus(status);
    } else {
      lastStatus = status;
    }

  } catch (error) {
    setConnectionState(false);
  } finally {
    statusRequestInFlight = false;
  }
}


// =====================================================
// Initial rendering
// =====================================================

applyProfile();
updateStepper();
loadHistory();
renderHistory();
renderRotationMap();
pollDeviceStatus();
setInterval(pollDeviceStatus, 200);

</script>

</body>
</html>
)rawliteral";


// =====================================================
// Web API
// =====================================================

String buildStatusJson() {

  float preDeltaRoll = angleDiffAbs(rollDeg, TARGET_ROLL);
  float preDeltaPitch = angleDiffAbs(pitchDeg, TARGET_PITCH);

  bool holdReferenceCaptured =
    state == HOLDING ||
    state == HOLD_COMPLETE ||
    state == RESULT_SUCCESS ||
    state == RESULT_INTERRUPTED;

  float holdDriftRoll = holdReferenceCaptured
    ? angleDiffAbs(rollDeg, holdStartRoll)
    : 0.0f;

  float holdDriftPitch = holdReferenceCaptured
    ? angleDiffAbs(pitchDeg, holdStartPitch)
    : 0.0f;

  String json;
  json.reserve(900);

  json += "{";
  json += "\"state\":\"";
  json += stateName(state);
  json += "\",\"issue\":\"";
  json += issueName(currentIssue);
  json += "\",";

  json += "\"imuInitOK\":";
  json += imuInitOK ? "true" : "false";
  json += ",\"imuReadOK\":";
  json += imuReadOK ? "true" : "false";

  json += ",\"padValue\":";
  json += String(fsrPadValue);
  json += ",\"padContact\":";
  json += padContact ? "true" : "false";
  json += ",\"penValue\":";
  json += String(fsrPenValue);
  json += ",\"penContact\":";
  json += penContact ? "true" : "false";
  json += ",\"contactConfirmed\":";
  json += contactConfirmed ? "true" : "false";

  json += ",\"roll\":";
  json += String(rollDeg, 2);
  json += ",\"pitch\":";
  json += String(pitchDeg, 2);
  json += ",\"preDeltaRoll\":";
  json += String(preDeltaRoll, 2);
  json += ",\"preDeltaPitch\":";
  json += String(preDeltaPitch, 2);
  json += ",\"preOrientationRawGood\":";
  json += preOrientationRawGood() ? "true" : "false";

  json += ",\"plungerDown\":";
  json += stablePlunger == LOW ? "true" : "false";
  json += ",\"pendingPlungerStart\":";
  json += pendingPlungerStart ? "true" : "false";

  json += ",\"holdElapsedMs\":";
  json += String(holdElapsedMs);
  json += ",\"holdTargetMs\":";
  json += String(holdTargetMs);
  json += ",\"holdStartRoll\":";
  json += String(holdStartRoll, 2);
  json += ",\"holdStartPitch\":";
  json += String(holdStartPitch, 2);
  json += ",\"holdDriftRoll\":";
  json += String(holdDriftRoll, 2);
  json += ",\"holdDriftPitch\":";
  json += String(holdDriftPitch, 2);
  json += ",\"holdStability\":\"";
  json += holdDriftWarning ? "WARNING" : "STABLE";
  json += "\",\"movementWarningCount\":";
  json += String(holdDriftWarningCount);
  json += ",\"maxHoldDriftRoll\":";
  json += String(maxHoldDriftRoll, 2);
  json += ",\"maxHoldDriftPitch\":";
  json += String(maxHoldDriftPitch, 2);

  json += ",\"greenLedOn\":";
  json += greenLedOn ? "true" : "false";
  json += ",\"redLedOn\":";
  json += redLedOn ? "true" : "false";
  json += ",\"vibrationActive\":";
  json += vibrationActive ? "true" : "false";
  json += ",\"buzzerActive\":";
  json += buzzerActive ? "true" : "false";
  json += "}";

  return json;
}


void sendJson(int statusCode, const String &payload) {
  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
  server.send(statusCode, "application/json; charset=UTF-8", payload);
}


void handleStatusApi() {
  sendJson(200, buildStatusJson());
}


void handleSessionStartApi() {

  if (state != WAIT_SITE) {
    sendJson(
      409,
      "{\"message\":\"진행 중인 세션이 있습니다. 새 세션으로 초기화한 뒤 다시 시작해주세요.\"}"
    );
    return;
  }

  unsigned long requestedHoldMs =
    (unsigned long)server.arg("holdMs").toInt();

  if (!setHoldTarget(requestedHoldMs)) {
    sendJson(
      400,
      "{\"message\":\"유지시간은 3초, 6초, 10초 중에서 선택해주세요.\"}"
    );
    return;
  }

  resetPreOrientationFilter();
  resetHoldDriftFilter();
  holdElapsedMs = 0;
  pendingPlungerStart = false;
  setState(READY);

  Serial.println(">>> WEB SESSION START");
  sendJson(200, buildStatusJson());
}


void handleSessionResetApi() {
  resetSession();
  Serial.println(">>> WEB SESSION RESET");
  sendJson(200, buildStatusJson());
}


// =====================================================
// ESP32 Setup
// =====================================================

void setup() {

  Serial.begin(115200);

  delay(300);

  analogReadResolution(12);

  pinMode(FSR_PAD_PIN, INPUT);
  pinMode(FSR_PEN_PIN, INPUT);
  pinMode(PLUNGER_PIN, INPUT_PULLUP);
  pinMode(VIBRATION_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);

  digitalWrite(VIBRATION_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  ledsOff();

  lastRawPlunger = digitalRead(PLUNGER_PIN);
  stablePlunger = lastRawPlunger;

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);

  imuInitOK = initIMU();

  Serial.println(imuInitOK ? "IMU INIT = OK" : "IMU INIT = ERROR");

  resetSession();

  Serial.println();
  Serial.println("Starting KOKCHI Embedded Web HMI v4.0...");

  WiFi.mode(WIFI_AP);

  bool apStarted =
    WiFi.softAP(AP_SSID, AP_PASSWORD);

  if (apStarted) {
    Serial.println("SoftAP started.");
  } else {
    Serial.println("SoftAP failed.");
  }

  Serial.print("SSID: ");
  Serial.println(AP_SSID);

  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP());

  server.on("/", []() {

    server.send_P(
      200,
      "text/html; charset=UTF-8",
      PAGE
    );

  });

  server.on("/api/status", HTTP_GET, handleStatusApi);
  server.on("/api/session/start", HTTP_POST, handleSessionStartApi);
  server.on("/api/session/reset", HTTP_POST, handleSessionResetApi);

  server.onNotFound([]() {
    sendJson(404, "{\"message\":\"요청한 경로를 찾을 수 없습니다.\"}");
  });

  server.begin();

  Serial.println("WebServer started.");
}


// =====================================================
// ESP32 Loop
// =====================================================

void loop() {

  imuReadOK = imuInitOK && readIMU(rollDeg, pitchDeg);

  fsrPadValue = analogRead(FSR_PAD_PIN);
  fsrPenValue = analogRead(FSR_PEN_PIN);
  padContact = fsrPadValue >= FSR_PAD_THRESHOLD;
  penContact = fsrPenValue >= FSR_PEN_THRESHOLD;
  contactConfirmed = padContact && penContact;

  updatePlunger();
  updateOutputs();
  handleSerialCommands();
  updateStateMachine();

  server.handleClient();

  printStatus();

}
