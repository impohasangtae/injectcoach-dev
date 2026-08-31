#include <Wire.h>
#include <math.h>

// ==============================
// Pin configuration
// ==============================

#define MPU_SDA 21
#define MPU_SCL 22

#define FSR_PAD_PIN 32
#define FSR_PEN_PIN 34

#define BUTTON_PIN 27

// ==============================
// MPU6050
// ==============================

const uint8_t MPU_ADDR = 0x68;

float rollDeg = 0.0;
float pitchDeg = 0.0;

// ==============================
// Button
// ==============================

bool buttonPressed = false;
bool previousButtonPressed = false;

unsigned long pressStartTime = 0;
float holdTimeSec = 0.0;

// ==============================
// Serial timing
// ==============================

unsigned long lastPrintTime = 0;


// ------------------------------
// MPU register write
// ------------------------------

void writeMPU(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}


// ------------------------------
// MPU initialization
// ------------------------------

bool initMPU6050() {

  Wire.begin(MPU_SDA, MPU_SCL);

  Wire.beginTransmission(MPU_ADDR);

  if (Wire.endTransmission() != 0) {
    return false;
  }

  // Wake MPU6050
  writeMPU(0x6B, 0x00);

  delay(100);

  return true;
}


// ------------------------------
// Read MPU6050
// ------------------------------

bool readMPU6050() {

  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B); // ACCEL_XOUT_H

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

  rollDeg =
    atan2(ay, az) * 180.0 / PI;

  pitchDeg =
    atan2(-ax, sqrt(ay * ay + az * az))
    * 180.0 / PI;

  return true;
}


// ------------------------------
// Button
// ------------------------------

void updateButton() {

  buttonPressed =
    (digitalRead(BUTTON_PIN) == LOW);

  if (buttonPressed && !previousButtonPressed) {

    pressStartTime = millis();
    holdTimeSec = 0.0;

    Serial.println("EVENT=BUTTON_DOWN");
  }

  if (buttonPressed) {

    holdTimeSec =
      (millis() - pressStartTime) / 1000.0;
  }

  if (!buttonPressed && previousButtonPressed) {

    Serial.print("EVENT=BUTTON_RELEASE,HOLD=");
    Serial.println(holdTimeSec, 2);
  }

  previousButtonPressed = buttonPressed;
}


// ==============================
// SETUP
// ==============================

void setup() {

  Serial.begin(115200);

  pinMode(FSR_PAD_PIN, INPUT);
  pinMode(FSR_PEN_PIN, INPUT);

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  Serial.println();
  Serial.println("==============================");
  Serial.println("KOKCHI INPUT INTEGRATION TEST");
  Serial.println("==============================");

  if (initMPU6050()) {

    Serial.println("MPU6050 = PASS");

  } else {

    Serial.println("MPU6050 = FAIL");
  }

  Serial.println();
}


// ==============================
// LOOP
// ==============================

void loop() {

  bool mpuOK = readMPU6050();

  int fsrPad = analogRead(FSR_PAD_PIN);
  int fsrPen = analogRead(FSR_PEN_PIN);

  updateButton();

  if (millis() - lastPrintTime >= 200) {

    lastPrintTime = millis();

    Serial.print("MPU=");
    Serial.print(mpuOK ? "OK" : "ERR");

    Serial.print(" | ROLL=");
    Serial.print(rollDeg, 1);

    Serial.print(" | PITCH=");
    Serial.print(pitchDeg, 1);

    Serial.print(" | FSR_PAD=");
    Serial.print(fsrPad);

    Serial.print(" | FSR_PEN=");
    Serial.print(fsrPen);

    Serial.print(" | PLUNGER=");
    Serial.print(buttonPressed ? "DOWN" : "UP");

    Serial.print(" | HOLD=");
    Serial.println(holdTimeSec, 2);
  }
}