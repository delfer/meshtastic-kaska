#include <Arduino.h>
#include <Wire.h>

// #define ENABLE_I2C_SCANNER

#ifdef ENABLE_I2C_SCANNER
#include "i2c_scanner.h"
TwoWire Wire2(PB14, PB13); // SDA, SCL
#endif

#define LED_PIN PA15 

void setup() {
  Serial.setTx(PA9);
  Serial.setRx(PA10);
  Serial.begin(115200);
  
  pinMode(LED_PIN, OUTPUT);
  Serial.println("Debug output initialized on PA9");

#ifdef ENABLE_I2C_SCANNER
  Wire2.begin();
#endif
}

void loop() {
#ifdef ENABLE_I2C_SCANNER
  scanI2C();
#endif
  digitalWrite(LED_PIN, HIGH);
  Serial.println("LED ON");
  delay(200);
  digitalWrite(LED_PIN, LOW);
  Serial.println("LED OFF");
  delay(10000);
}
