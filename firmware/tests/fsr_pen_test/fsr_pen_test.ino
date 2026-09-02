const int FSR_PEN_PIN = 34;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("FSR-PEN TEST START");
}

void loop() {
  int fsrPen = analogRead(FSR_PEN_PIN);

  Serial.print("FSR_PEN = ");
  Serial.println(fsrPen);

  delay(200);
}