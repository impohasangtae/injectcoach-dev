#include <Wire.h>
#include <math.h>

// =====================================================
// KOKCHI - Final Grip Orientation Validation
//
// 0 = REFERENCE
// 1 = TIP_UP
// 2 = TIP_DOWN
// 3 = TILT_LEFT
// 4 = TILT_RIGHT
//
// 각 조건 3회
// =====================================================

const uint8_t IMU_ADDR = 0x68;

const int SDA_PIN = 21;
const int SCL_PIN = 22;

const int TRIALS = 3;

const int SAMPLES_PER_CAPTURE = 25;
const int SAMPLE_DELAY_MS = 20;


// -----------------------------------------------------
// 저장
// -----------------------------------------------------

float refRoll[TRIALS];
float refPitch[TRIALS];

int refCount = 0;
int upCount = 0;
int downCount = 0;
int leftCount = 0;
int rightCount = 0;


// 현재 최종 기준 후보
float targetRoll = 0.0;
float targetPitch = 0.0;
bool referenceComplete = false;


// =====================================================
// IMU INIT
// =====================================================

bool initIMU() {

  Wire.beginTransmission(IMU_ADDR);
  Wire.write(0x6B);
  Wire.write(0x00);

  if (Wire.endTransmission() != 0) {
    return false;
  }

  delay(100);

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
// 평균 자세 측정
// =====================================================

bool capturePose(float &avgRoll, float &avgPitch) {

  float sumRoll = 0.0;
  float sumPitch = 0.0;

  int valid = 0;


  for (int i = 0; i < SAMPLES_PER_CAPTURE; i++) {

    float roll;
    float pitch;

    if (readIMU(roll, pitch)) {

      sumRoll += roll;
      sumPitch += pitch;
      valid++;
    }

    delay(SAMPLE_DELAY_MS);
  }


  if (valid == 0) {
    return false;
  }


  avgRoll = sumRoll / valid;
  avgPitch = sumPitch / valid;

  return true;
}


// =====================================================
// Reference 계산
// =====================================================

void calculateReference() {

  float rollSum = 0.0;
  float pitchSum = 0.0;


  for (int i = 0; i < TRIALS; i++) {

    rollSum += refRoll[i];
    pitchSum += refPitch[i];
  }


  targetRoll =
      rollSum / TRIALS;

  targetPitch =
      pitchSum / TRIALS;


  referenceComplete = true;


  Serial.println();
  Serial.println("================================");
  Serial.println("FINAL GRIP REFERENCE");
  Serial.println("================================");

  Serial.print("TARGET_ROLL = ");
  Serial.println(targetRoll, 2);

  Serial.print("TARGET_PITCH = ");
  Serial.println(targetPitch, 2);

  Serial.println("================================");
}


// =====================================================
// Reference 측정
// =====================================================

void measureReference() {

  if (refCount >= TRIALS) {

    Serial.println("REFERENCE already complete.");
    return;
  }


  Serial.println();
  Serial.println("Hold FINAL marked grip reference pose...");
  delay(500);


  float roll;
  float pitch;


  if (!capturePose(roll, pitch)) {

    Serial.println("IMU READ ERROR");
    return;
  }


  refRoll[refCount] = roll;
  refPitch[refCount] = pitch;

  refCount++;


  Serial.print("REFERENCE ");
  Serial.print(refCount);
  Serial.print("/");
  Serial.print(TRIALS);

  Serial.print(" | ROLL=");
  Serial.print(roll, 2);

  Serial.print(" | PITCH=");
  Serial.println(pitch, 2);


  if (refCount == TRIALS) {
    calculateReference();
  }
}


// =====================================================
// Error pose 측정
// =====================================================

void measureError(
  const char* label,
  int &counter
) {

  if (!referenceComplete) {

    Serial.println();
    Serial.println("FIRST complete REFERENCE 3 times using 0.");
    return;
  }


  if (counter >= TRIALS) {

    Serial.print(label);
    Serial.println(" already complete.");
    return;
  }


  Serial.println();
  Serial.print("Hold pose: ");
  Serial.println(label);

  delay(500);


  float roll;
  float pitch;


  if (!capturePose(roll, pitch)) {

    Serial.println("IMU READ ERROR");
    return;
  }


  counter++;


  float deltaRoll =
      fabs(roll - targetRoll);

  float deltaPitch =
      fabs(pitch - targetPitch);


  Serial.println();

  Serial.print(label);
  Serial.print(" ");
  Serial.print(counter);
  Serial.print("/");
  Serial.println(TRIALS);


  Serial.print("ROLL = ");
  Serial.println(roll, 2);

  Serial.print("PITCH = ");
  Serial.println(pitch, 2);

  Serial.print("Delta Roll  = ");
  Serial.print(deltaRoll, 2);
  Serial.println(" deg");

  Serial.print("Delta Pitch = ");
  Serial.print(deltaPitch, 2);
  Serial.println(" deg");

  Serial.println("------------------------------");
}


// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(115200);
  delay(500);

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);


  Serial.println();
  Serial.println("======================================");
  Serial.println("KOKCHI FINAL GRIP VALIDATION");
  Serial.println("======================================");


  if (!initIMU()) {

    Serial.println("IMU INIT ERROR");

    while (true) {
      delay(1000);
    }
  }


  Serial.println();
  Serial.println("IMPORTANT:");
  Serial.println("- Use the marked GRIP position");
  Serial.println("- Keep the UP mark facing the same direction");
  Serial.println();

  Serial.println("0 = REFERENCE pose");
  Serial.println("1 = TIP UP");
  Serial.println("2 = TIP DOWN");
  Serial.println("3 = TILT LEFT");
  Serial.println("4 = TILT RIGHT");

  Serial.println();
  Serial.println("Each condition = 3 trials");
}


// =====================================================
// LOOP
// =====================================================

void loop() {

  if (!Serial.available()) {
    return;
  }


  char c = Serial.read();


  if (c == '0') {

    measureReference();
  }


  else if (c == '1') {

    measureError(
      "TIP_UP",
      upCount
    );
  }


  else if (c == '2') {

    measureError(
      "TIP_DOWN",
      downCount
    );
  }


  else if (c == '3') {

    measureError(
      "TILT_LEFT",
      leftCount
    );
  }


  else if (c == '4') {

    measureError(
      "TILT_RIGHT",
      rightCount
    );
  }


  if (
    refCount >= TRIALS &&
    upCount >= TRIALS &&
    downCount >= TRIALS &&
    leftCount >= TRIALS &&
    rightCount >= TRIALS
  ) {

    Serial.println();
    Serial.println("======================================");
    Serial.println("FINAL GRIP VALIDATION COMPLETE");
    Serial.println("======================================");

    // 반복 출력 방지
    refCount++;
    upCount++;
    downCount++;
    leftCount++;
    rightCount++;
  }
}