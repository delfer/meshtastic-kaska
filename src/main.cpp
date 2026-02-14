#include <Arduino.h>

#define LED_PIN PA15 

void setup() {
  // Используем Serial1, так как мы включили ENABLE_HWSERIAL1
  Serial1.setTx(PA9);
  Serial1.setRx(PA10);
  Serial1.begin(115200);
  
  pinMode(LED_PIN, OUTPUT);
  Serial1.println("Debug output initialized on PA9");
}

void loop() {
  digitalWrite(LED_PIN, HIGH);
  Serial1.println("LED ON");
  delay(200);
  digitalWrite(LED_PIN, LOW);
  Serial1.println("LED OFF");
  delay(10000);
}
