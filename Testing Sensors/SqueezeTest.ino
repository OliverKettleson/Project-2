const int fsrPin = A5;

void setup() {
  Serial.begin(115200);
}

void loop() {
  int fsrValue = analogRead(fsrPin);

  Serial.print("FSR value: ");
  Serial.print(fsrValue);
  Serial.print("   Status: ");

  if (fsrValue < 100) { 
    Serial.println("No grip");
  } 
  else if (fsrValue < 300) {
    Serial.println("Light grip");
  } 
  else if (fsrValue < 500) {
    Serial.println("Medium grip");
  } 
  else {
    Serial.println("Strong grip");
  }

  delay(50);
}