#include <Wire.h>
#include <math.h>

const uint8_t MPU_ADDR = 0x68;

void setup() {
  Serial.begin(115200);
  delay(1000);

  Wire.begin(21, 22);

  // MPU6050 wake up
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);       // PWR_MGMT_1
  Wire.write(0x00);       // Sleep 해제
  Wire.endTransmission();

  delay(500);

  Serial.println("=== MPU6050 SENSOR TEST ===");
}

void loop() {
  int16_t ax, ay, az;
  int16_t gx, gy, gz;
  int16_t tempRaw;

  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);  // ACCEL_XOUT_H
  Wire.endTransmission(false);

  Wire.requestFrom(MPU_ADDR, (uint8_t)14, true);

  if (Wire.available() == 14) {
    ax = (Wire.read() << 8) | Wire.read();
    ay = (Wire.read() << 8) | Wire.read();
    az = (Wire.read() << 8) | Wire.read();

    tempRaw = (Wire.read() << 8) | Wire.read();

    gx = (Wire.read() << 8) | Wire.read();
    gy = (Wire.read() << 8) | Wire.read();
    gz = (Wire.read() << 8) | Wire.read();

    float roll = atan2((float)ay, (float)az) * 180.0 / PI;
    float pitch = atan2(
      -(float)ax,
      sqrt((float)ay * ay + (float)az * az)
    ) * 180.0 / PI;

    Serial.print("ACC = ");
    Serial.print(ax);
    Serial.print(", ");
    Serial.print(ay);
    Serial.print(", ");
    Serial.print(az);

    Serial.print("   GYRO = ");
    Serial.print(gx);
    Serial.print(", ");
    Serial.print(gy);
    Serial.print(", ");
    Serial.print(gz);

    Serial.print("   ROLL = ");
    Serial.print(roll, 1);

    Serial.print("   PITCH = ");
    Serial.println(pitch, 1);

  } else {
    Serial.println("MPU READ ERROR");
  }

  delay(300);
}