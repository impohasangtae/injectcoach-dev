#include <Wire.h>
#include <math.h>

// =====================================================
// KOKCHI - MPU Error Pose Test
// Reference:
// TARGET_ROLL  = 4.70
// TARGET_PITCH = 43.67
// =====================================================

const uint8_t IMU_ADDR = 0x68;

const int SDA_PIN = 21;
const int SCL_PIN = 22;

const float TARGET_ROLL  = 4.70;
const float TARGET_PITCH = 43.67;

const int TRIALS_PER_DIRECTION = 3;

const int SAMPLES_PER_CAPTURE = 25;
const int SAMPLE_DELAY_MS = 20;


// 1 = tip UP
// 2 = tip DOWN
// 3 = LEFT tilt
// 4 = RIGHT tilt

int count1 = 0;
int count2 = 0;
int count3 = 0;
int count4 = 0;


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
// AVERAGE POSE
// =====================================================

bool capturePose(float &avgRoll, float &avgPitch) {

  float rollSum = 0.0;
  float pitchSum = 0.0;

  int valid = 0;


  for (int i = 0; i < SAMPLES_PER_CAPTURE; i++) {

    float roll;
    float pitch;

    if (readIMU(roll, pitch)) {

      rollSum += roll;
      pitchSum += pitch;

      valid++;
    }

    delay(SAMPLE_DELAY_MS);
  }


  if (valid == 0) {
    return false;
  }


  avgRoll = rollSum / valid;
  avgPitch = pitchSum / valid;

  return true;
}


// =====================================================
// RESULT PRINT
// =====================================================

void printResult(
  const char* label,
  int trial,
  float roll,
  float pitch
) {

  float deltaRoll =
      fabs(roll - TARGET_ROLL);

  float deltaPitch =
      fabs(pitch - TARGET_PITCH);


  Serial.println();

  Serial.print(label);

  Serial.print(" ");
  Serial.print(trial);
  Serial.print("/");
  Serial.println(TRIALS_PER_DIRECTION);


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
// CAPTURE ONE ERROR POSE
// =====================================================

void measureDirection(
  const char* label,
  int &counter
) {

  if (counter >= TRIALS_PER_DIRECTION) {

    Serial.print(label);
    Serial.println(" already complete.");

    return;
  }


  Serial.println();
  Serial.print("Hold pose: ");
  Serial.println(label);

  // 자세 안정화 시간
  delay(500);


  float roll;
  float pitch;


  if (!capturePose(roll, pitch)) {

    Serial.println("IMU READ ERROR");

    return;
  }


  counter++;

  printResult(
    label,
    counter,
    roll,
    pitch
  );
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
  Serial.println("KOKCHI MPU ERROR POSE TEST");
  Serial.println("======================================");

  if (!initIMU()) {

    Serial.println("IMU INIT ERROR");

    while (true) {
      delay(1000);
    }
  }


  Serial.println();
  Serial.println("REFERENCE:");
  Serial.println("Roll  = 4.70 deg");
  Serial.println("Pitch = 43.67 deg");

  Serial.println();
  Serial.println("COMMANDS");

  Serial.println("1 = Syringe tip UP");
  Serial.println("2 = Syringe tip DOWN");
  Serial.println("3 = Tilt LEFT");
  Serial.println("4 = Tilt RIGHT");

  Serial.println();
  Serial.println("Each direction: 3 independent trials");
  Serial.println();

  Serial.println("IMPORTANT:");
  Serial.println("Make a clearly wrong pose,");
  Serial.println("but not an extreme/unrealistic pose.");
}


// =====================================================
// LOOP
// =====================================================

void loop() {

  if (!Serial.available()) {
    return;
  }


  char c = Serial.read();


  if (c == '1') {

    measureDirection(
      "TIP_UP",
      count1
    );
  }


  else if (c == '2') {

    measureDirection(
      "TIP_DOWN",
      count2
    );
  }


  else if (c == '3') {

    measureDirection(
      "TILT_LEFT",
      count3
    );
  }


  else if (c == '4') {

    measureDirection(
      "TILT_RIGHT",
      count4
    );
  }


  if (
    count1 == TRIALS_PER_DIRECTION &&
    count2 == TRIALS_PER_DIRECTION &&
    count3 == TRIALS_PER_DIRECTION &&
    count4 == TRIALS_PER_DIRECTION
  ) {

    Serial.println();
    Serial.println("======================================");
    Serial.println("ALL ERROR POSE TRIALS COMPLETE");
    Serial.println("======================================");

    // 다시 반복 출력되는 것 방지
    count1++;
    count2++;
    count3++;
    count4++;
  }
}