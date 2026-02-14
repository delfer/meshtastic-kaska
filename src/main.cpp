#include <Arduino.h>
#include <Wire.h>
#include <STM32LowPower.h>

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

  // Инициализация библиотеки энергосбережения
  LowPower.begin();

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
  delay(200); // while true; do st-flash erase && break ; done
  digitalWrite(LED_PIN, LOW);
  Serial.println("LED OFF");

  // Мгновенное пробуждение обычно вызвано тем, что UART продолжает генерировать прерывания
  // или линия RX "шумит" при переходе в спящий режим.
  
  // 1. Ждем завершения передачи всех данных по Serial
  Serial.flush();
  
  // 2. Выключаем Serial перед сном, чтобы его прерывания не будили МК.
  // В STM32L0 UART может будить процессор, если это не настроено специально,
  // но чаще он просто мешает уйти в глубокий сон.
  // Serial.end();

  // 3. Переводим в deepSleep (режим STOP). В этом режиме SysTick (1мс таймер Arduino) остановлен.
  // Пробуждение произойдет по внутреннему RTC.
  LowPower.deepSleep(10000);

  // 4. После пробуждения восстанавливаем Serial для вывода отладки
  // Serial.begin(115200);
}
