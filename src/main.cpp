#include <Arduino.h>
#include <Preferences.h>
#include "ThermistorCalc.h"

ThermistorCalc *thermistor;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  thermistor = new ThermistorCalc(-40, 167820, 25, 6523, 120, 302);
}

void loop() {
  // put your main code here, to run repeatedly:
  Serial.println("R=7701"); 
  double c = thermistor->celsiusFromResistance(7701);
  Serial.println((String) "c=" + c);
  double r = thermistor->resistanceFromCelsius(21);
  Serial.println((String) "r=" + r);
  delay(2000);
}