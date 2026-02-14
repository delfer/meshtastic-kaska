#include <Arduino.h>
#include <Wire.h>
#include <STM32LowPower.h>
#include <RadioLib.h>

// #define ENABLE_I2C_SCANNER

#ifdef ENABLE_I2C_SCANNER
#include "i2c_scanner.h"
TwoWire Wire2(PB14, PB13); // SDA, SCL
#endif

#define LED_PIN PA15

// LoRa Sx1276 pins from README.md
#define LORA_SCK  PA5
#define LORA_MISO PA6
#define LORA_MOSI PA7
#define LORA_NSS  PA4
#define LORA_RST  PB15
#define LORA_DIO0 PB2
#define LORA_DIO1 PB1

SX1276 radio = new Module(LORA_NSS, LORA_DIO0, LORA_RST, LORA_DIO1);

void setup() {
  Serial.setTx(PA9);
  Serial.setRx(PA10);
  Serial.begin(115200);
  
  pinMode(LED_PIN, OUTPUT);
  Serial.println("Debug output initialized on PA9");

  // Инициализация библиотеки энергосбережения
  // LowPower.begin();

#ifdef ENABLE_I2C_SCANNER
  Wire2.begin();
#endif

  // LoRa initialization
  Serial.print(F("[RadioLib] Initializing ... "));
  SPI.setSCLK(LORA_SCK);
  SPI.setMISO(LORA_MISO);
  SPI.setMOSI(LORA_MOSI);
  SPI.begin();
  // 1. Инициализация с базовыми параметрами
  // Частота: 869.075, Полоса: 250.0, SF: 11, CR: 5 (Long Fast)
  int state = radio.begin(869.075, 250.0, 11, 5);
  if (state == RADIOLIB_ERR_NONE) {
    // 2. Устанавливаем специфичный для Meshtastic Sync Word
    state = radio.setSyncWord(0x2B);
    
    // 3. Устанавливаем длину преамбулы (у Meshtastic это 16)
    state = radio.setPreambleLength(16);

    // 4. Опционально: CRC должен быть включен (в RadioLib по умолчанию включен)
    state = radio.setCRC(true);

    if (state == RADIOLIB_ERR_NONE) {
        Serial.println(F("Meshtastic configuration applied!"));
    }
    Serial.println(F("success!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
    while (true);
  }

  // Проверяем корректность
  byte version = radio.getChipVersion();
  Serial.print(F("Chip version: 0x"));
  Serial.println(version, HEX);

  if (version != 0x12) {
    Serial.println(F("ОШИБКА: Чип не отвечает или это не SX1276!"));
  }

  // Настройка дополнительных параметров (CR задается в begin, но можно уточнить)
  // radio.setCodingRate(5); // Уже задано в begin как 4/5 (значение 5)

  // Переводим в режим приема
  Serial.print(F("[RadioLib] Starting to listen ... "));
  state = radio.startReceive();
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("success!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
    while (true);
  }

  // Print debug info
  Serial.println(F("--- RadioLib Info ---"));
  Serial.println(F("Radio Chip: SX1276"));
  Serial.print(F("NSS Pin: ")); Serial.println(LORA_NSS);
  Serial.print(F("DIO0 Pin: ")); Serial.println(LORA_DIO0);
  Serial.print(F("RST Pin: ")); Serial.println(LORA_RST);
  Serial.println(F("---------------------"));
}

void loop() {
  // Проверяем, пришел ли пакет по состоянию прерывания на DIO0
  // RadioLib устанавливает флаг, когда на DIO0 высокий уровень
  if (digitalRead(LORA_DIO0) == HIGH) { 
    String str;
    int state = radio.readData(str);

    if (state == RADIOLIB_ERR_NONE) {
      Serial.println(F("[RadioLib] Received packet!"));
      Serial.print(F("[RadioLib] Data: ")); Serial.println(str);
      Serial.print(F("[RadioLib] RSSI: ")); Serial.print(radio.getRSSI()); Serial.println(F(" dBm"));
    } else {
      Serial.print(F("[RadioLib] Error: ")); Serial.println(state);
    }

    // ВАЖНО: После чтения нужно СНОВА вызвать startReceive()
    radio.startReceive();
  }
}