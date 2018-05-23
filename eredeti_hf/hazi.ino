#include "SmartBox.h"

void setup() {
  Serial.begin(9600);
  while (!Serial) { }
  Serial.print("Listening... ");
  //smartBox = SmartBox();
}

void loop() {
  Serial.print("Listening... ");
  SmartBox smartBox = SmartBox();
  smartBox.listenBeeping();
}
