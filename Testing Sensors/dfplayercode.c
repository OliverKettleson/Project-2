#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>

SoftwareSerial mySerial(4,5); // RX, TX
DFRobotDFPlayerMini myDFPlayer;

void setup() {
  Serial.begin(9600);
  mySerial.begin(9600);

  if (!myDFPlayer.begin(mySerial)) {
    Serial.println("DFPlayer not detected!");
    while (true);
  }

  Serial.println("DFPlayer ready");

  myDFPlayer.volume(20);   // volume (0–30)
  delay(1000);

  myDFPlayer.play(1);      // plays 0001.mp3
}

void loop() {
  // nothing needed
}
