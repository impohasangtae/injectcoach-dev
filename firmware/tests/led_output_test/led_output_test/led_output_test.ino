const int BUZZER_PIN = 25;

void setup() {
  Serial.begin(115200);

  pinMode(BUZZER_PIN, OUTPUT);

  // 시작할 때 반드시 OFF
  digitalWrite(BUZZER_PIN, LOW);

  Serial.println("KOKCHI BUZZER TEST");
}

void loop() {

  Serial.println("BUZZER ON");
  digitalWrite(BUZZER_PIN, HIGH);

  delay(100);   // 짧게 삑!

  Serial.println("BUZZER OFF");
  digitalWrite(BUZZER_PIN, LOW);

  delay(1900);  // 1.9초 기다림
}