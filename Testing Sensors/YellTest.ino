int micPin = A0;
int ledPin = 8;
int threshold = 700;

void setup() {
  pinMode(ledPin, OUTPUT);
  Serial.begin(115200);
}

void loop() {

  int signalMax = 0;
  int signalMin = 1023;

  unsigned long start = millis();

  // Sample window (50 ms)
  while (millis() - start < 50) {

    int sample = analogRead(micPin);

    if (sample > signalMax) signalMax = sample;
    if (sample < signalMin) signalMin = sample;
  }

  int peakToPeak = signalMax - signalMin;

  Serial.print("p2p: ");
  Serial.println(peakToPeak);

  if (peakToPeak > threshold) {
    digitalWrite(ledPin, HIGH);
    delay(100);   // LED stays on briefly
  }
  else {
    digitalWrite(ledPin, LOW);
  }
}