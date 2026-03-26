void setup() {
  Serial.begin(115200);
}

void loop() {

  int hall = digitalRead(9);
  int vib = analogRead(A1);

  Serial.print("Hall: ");
  Serial.print(hall);

  Serial.print("   Vib: ");
  Serial.println(vib);

  delay(50);
}