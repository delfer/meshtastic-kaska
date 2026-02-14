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
  int state = radio.begin(869.080, 250.0, 11, 5);
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
if (digitalRead(LORA_DIO0) == HIGH) {
    size_t len = radio.getPacketLength();
    uint8_t buffer[256]; // Буфер для пакета

    int state = radio.readData(buffer, len);

    if (state == RADIOLIB_ERR_NONE) {
        digitalWrite(LED_PIN, HIGH);
        Serial.println(F("\n--- [Meshtastic Packet Insight] ---"));

        if (len >= 16) {
            // Базовые ID (уже знакомы)
            uint32_t dest   = (uint32_t)buffer[0] | (uint32_t)buffer[1] << 8 | (uint32_t)buffer[2] << 16 | (uint32_t)buffer[3] << 24;
            uint32_t sender = (uint32_t)buffer[4] | (uint32_t)buffer[5] << 8 | (uint32_t)buffer[6] << 16 | (uint32_t)buffer[7] << 24;
            
            // Разбор флагов (Байт 12)
            uint8_t flags   = buffer[12];
            uint8_t hopLimit = flags & 0b00000111;           // Биты 0-2
            bool wantAck     = (flags >> 3) & 0x01;          // Бит 3
            bool viaMqtt     = (flags >> 4) & 0x01;          // Бит 4
            uint8_t priority = (flags >> 5) & 0b00000111;    // Биты 5-7

            // Канал (Байт 13)
            uint8_t chanHash = buffer[13];

            Serial.print(F("Sender ID:    0x")); Serial.println(sender, HEX);
            Serial.print(F("Dest ID:      0x")); Serial.print(dest, HEX); 
            if (dest == 0xFFFFFFFF) Serial.println(F(" (Broadcast)")); else Serial.println();

            Serial.print(F("Hop Limit:    ")); Serial.println(hopLimit);
            Serial.print(F("Priority:     ")); Serial.println(priority);
            Serial.print(F("Want ACK:     ")); Serial.println(wantAck ? F("Yes") : F("No"));
            Serial.print(F("Via MQTT:     ")); Serial.println(viaMqtt ? F("Yes") : F("No"));
            Serial.print(F("Chan Hash:    0x")); Serial.println(chanHash, HEX);

            // Отладочные данные железа
            Serial.print(F("Freq Error:   ")); Serial.print(radio.getFrequencyError()); Serial.println(F(" Hz"));
            Serial.print(F("Payload Size: ")); Serial.print(len - 16); Serial.println(F(" bytes"));
            
            // Дамп первых 8 байт зашифрованной части (для интереса)
            Serial.print(F("Payload Hex:  "));
            for(int i = 16; i < min((int)len, 24); i++) {
                if(buffer[i] < 0x10) Serial.print('0');
                Serial.print(buffer[i], HEX);
                Serial.print(' ');
            }
            Serial.println(F("..."));
        }

        Serial.print(F("RSSI/SNR:    ")); Serial.print(radio.getRSSI()); 
        Serial.print(F(" / ")); Serial.println(radio.getSNR());
        digitalWrite(LED_PIN, LOW);
    }
    
    radio.startReceive();
}
}