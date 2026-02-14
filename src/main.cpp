#include <Arduino.h>

#define LED_PIN PA15 

void setup() {
  Serial.setTx(PA9);
  Serial.setRx(PA10);
  Serial.begin(115200);
  
  pinMode(LED_PIN, OUTPUT);
  Serial.println("Debug output initialized on PA9");
}

void loop() {
  digitalWrite(LED_PIN, HIGH);
  Serial.println("LED ON");
  delay(200);
  digitalWrite(LED_PIN, LOW);
  Serial.println("LED OFF");
  delay(10000);
}
