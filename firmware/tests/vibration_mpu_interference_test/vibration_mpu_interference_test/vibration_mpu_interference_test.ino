const int GREEN_LED_PIN = 33;
const int RED_LED_PIN   = 14;

void setup() {
  Serial.begin(115200);

  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);

  digitalWrite(GREEN_LED_PIN, LOW);
  digitalWrite(RED_LED_PIN, LOW);

  Serial.println("KOKCHI LED TEST");
}

void loop() {

  // 1. 초록 LED
  Serial.println("GREEN ON");
  digitalWrite(GREEN_LED_PIN, HIGH);
  digitalWrite(RED_LED_PIN, LOW);
  delay(1000);

  // 2. 모두 OFF
  Serial.println("ALL OFF");
  digitalWrite(GREEN_LED_PIN, LOW);
  digitalWrite(RED_LED_PIN, LOW);
  delay(500);

  // 3. 빨강 LED
  Serial.println("RED ON");
  digitalWrite(GREEN_LED_PIN, LOW);
  digitalWrite(RED_LED_PIN, HIGH);
  delay(1000);

  // 4. 모두 OFF
  Serial.println("ALL OFF");
  digitalWrite(GREEN_LED_PIN, LOW);
  digitalWrite(RED_LED_PIN, LOW);
  delay(500);
}