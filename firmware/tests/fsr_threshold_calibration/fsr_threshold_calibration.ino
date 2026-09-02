#include <Wire.h>
#include <math.h>

// =============================================
// KOKCHI - MPU Reference Pose Calibration
// =============================================

const uint8_t IMU_ADDR = 0x68;

const int SDA_PIN = 21;
const int SCL_PIN = 22;

const int TRIALS = 10;

const int SAMPLES_PER_CAPTURE = 25;
const int SAMPLE_DELAY_MS = 20;


// =============================================
// 저장
// =============================================

float rolls[TRIALS];
float pitches[TRIALS];

int trialCount = 0;


// =============================================
// IMU 초기화
// =============================================

bool initIMU() {

  Wire.beginTransmission(IMU_ADDR);
  Wire.write(0x6B);
  Wire.write(0x00);

  if (Wire.endTransmission() != 0) {
    return false;
  }

  delay(100);

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
  Serial.println(whoAmI, HEX);

  return true;
}


// =============================================
// Roll / Pitch 읽기
// =============================================

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


// =============================================
// 한 자세 평균 측정
// =============================================

bool capturePose(float &avgRoll, float &avgPitch) {

  float rollSum = 0.0;
  float pitchSum = 0.0;

  int validSamples = 0;


  for (int i = 0;
       i < SAMPLES_PER_CAPTURE;
       i++) {

    float roll;
    float pitch;

    if (readIMU(roll, pitch)) {

      rollSum += roll;
      pitchSum += pitch;

      validSamples++;
    }

    delay(SAMPLE_DELAY_MS);
  }


  if (validSamples == 0) {
    return false;
  }


  avgRoll =
      rollSum / validSamples;

  avgPitch =
      pitchSum / validSamples;

  return true;
}


// =============================================
// 평균
// =============================================

float getMean(float values[], int count) {

  float sum = 0.0;

  for (int i = 0; i < count; i++) {
    sum += values[i];
  }

  return sum / count;
}


// =============================================
// Min / Max
// =============================================

float getMin(float values[], int count) {

  float v = values[0];

  for (int i = 1; i < count; i++) {
    if (values[i] < v) {
      v = values[i];
    }
  }

  return v;
}


float getMax(float values[], int count) {

  float v = values[0];

  for (int i = 1; i < count; i++) {
    if (values[i] > v) {
      v = values[i];
    }
  }

  return v;
}


// =============================================
// SUMMARY
// =============================================

void printSummary() {

  if (trialCount == 0) {
    return;
  }


  float meanRoll =
      getMean(rolls, trialCount);

  float meanPitch =
      getMean(pitches, trialCount);


  float minRoll =
      getMin(rolls, trialCount);

  float maxRoll =
      getMax(rolls, trialCount);

  float minPitch =
      getMin(pitches, trialCount);

  float maxPitch =
      getMax(pitches, trialCount);


  Serial.println();
  Serial.println("================================");
  Serial.println("KOKCHI MPU CALIBRATION SUMMARY");
  Serial.println("================================");

  Serial.print("TARGET_ROLL candidate = ");
  Serial.println(meanRoll, 2);

  Serial.print("ROLL range = ");
  Serial.print(minRoll, 2);
  Serial.print(" ~ ");
  Serial.println(maxRoll, 2);

  Serial.println();

  Serial.print("TARGET_PITCH candidate = ");
  Serial.println(meanPitch, 2);

  Serial.print("PITCH range = ");
  Serial.print(minPitch, 2);
  Serial.print(" ~ ");
  Serial.println(maxPitch, 2);

  Serial.println("================================");
}


// =============================================
// SETUP
// =============================================

void setup() {

  Serial.begin(115200);
  delay(500);

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);


  Serial.println();
  Serial.println("================================");
  Serial.println("KOKCHI MPU REFERENCE CALIBRATION");
  Serial.println("================================");


  if (!initIMU()) {

    Serial.println("IMU INIT ERROR");

    while (true) {
      delay(1000);
    }
  }


  Serial.println();
  Serial.println("REFERENCE POSE:");
  Serial.println("- Syringe perpendicular to pad/body surface");
  Serial.println("- Syringe parallel to ground");

  Serial.println();
  Serial.println("HOW TO:");
  Serial.println("1. Make the reference pose.");
  Serial.println("2. Keep it still.");
  Serial.println("3. Type 1 + Enter.");
  Serial.println("4. Lift syringe and repeat.");
  Serial.println("5. Complete 10 independent trials.");
  Serial.println();

  Serial.println("S = Show summary");
}


// =============================================
// LOOP
// =============================================

void loop() {

  if (!Serial.available()) {
    return;
  }


  char c = Serial.read();


  if (c == '1') {

    if (trialCount >= TRIALS) {

      Serial.println(
        "All 10 reference trials already complete."
      );

      return;
    }


    Serial.println();
    Serial.println("Hold reference pose...");
    delay(500);


    float avgRoll;
    float avgPitch;


    if (capturePose(avgRoll, avgPitch)) {

      rolls[trialCount] = avgRoll;
      pitches[trialCount] = avgPitch;

      trialCount++;


      Serial.print("REFERENCE ");
      Serial.print(trialCount);
      Serial.print("/");
      Serial.print(TRIALS);

      Serial.print(" | ROLL=");
      Serial.print(avgRoll, 2);

      Serial.print(" | PITCH=");
      Serial.println(avgPitch, 2);


      if (trialCount == TRIALS) {

        printSummary();

        Serial.println();
        Serial.println("REFERENCE CALIBRATION COMPLETE.");
      }
    }

    else {

      Serial.println("IMU READ ERROR");
    }
  }


  else if (c == 's' || c == 'S') {

    printSummary();
  }
}