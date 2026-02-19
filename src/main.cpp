#include <Arduino.h>
#include <Wire.h>
#include <STM32LowPower.h>
#include <RadioLib.h>

// #define ENABLE_I2C_SCANNER
#define ENABLE_PACKET_DEBUG

#ifdef ENABLE_I2C_SCANNER
#include "i2c_scanner.h"
TwoWire Wire2(PB14, PB13); // SDA, SCL
#endif

#ifdef ENABLE_PACKET_DEBUG
#include "packet_debug.h"
#endif
#include "packet_cache.h"
#include "mesh_utils.h"

#define LED_PIN PA15

// LoRa Sx1276 pins from README.md
#define LORA_SCK  PA5
#define LORA_MISO PA6
#define LORA_MOSI PA7
#define LORA_NSS  PA4
#define LORA_RST  PB15
#define LORA_DIO0 PB2
#define LORA_DIO1 PB1

#define BAT_PIN PA3

SX1276 radio = new Module(LORA_NSS, LORA_DIO0, LORA_RST, LORA_DIO1);

float readBatteryVoltage() {
  // Теперь АЦП успевает заряжаться благодаря ADC_SAMPLINGTIME в platformio.ini
  analogReadResolution(12);

  // Одно чтение теперь гораздо точнее благодаря увеличению ADC_SAMPLINGTIME
  uint32_t raw = analogRead(BAT_PIN);

  // Теоретический коэффициент для делителя 1/2 и Vref 2.5V:
  // (2.5 / 4095) * 2 = 0.00122100122
  // Фактически при 4.01В на входе АЦП выдает raw=2288.
  // Это означает, что реальный коэффициент составляет 4.01 / 2288 ≈ 0.00175262.
  // Разница может быть вызвана отклонением Vref от 2.5В или погрешностью резисторов.
  float voltage = raw * 0.00175262f;

  Serial.print(F("\nADC Raw: "));
  Serial.print(raw);
  Serial.print(F(" -> "));

  return voltage;
}

void setup() {
  Serial.setTx(PA9);
  Serial.setRx(PA10);
  Serial.begin(115200);
  
  pinMode(LED_PIN, OUTPUT);
  pinMode(BAT_PIN, INPUT_ANALOG);
  Serial.println("Debug output initialized on PA9");

  // Инициализация библиотеки энергосбережения
  LowPower.begin();
  // Настройка пробуждения по прерыванию на DIO0 (RISING)
  LowPower.attachInterruptWakeup(LORA_DIO0, NULL, RISING, DEEP_SLEEP_MODE);

  // Включить тактирование DBGMCU (обязательно для L0!)
  // (Потребление вырастет до ~300 мкА, но SWD не отвалится)
  // (Без него потребление будет минимальным ~0.8 мкА)
  __HAL_RCC_DBGMCU_CLK_ENABLE(); 
  HAL_DBGMCU_EnableDBGStopMode();

#ifdef ENABLE_I2C_SCANNER
  Wire2.begin();
#endif

  Serial.println(F("Booting..."));
  // Инициализация кэша пакетов
  packetCacheInit();
  Serial.println(F("Cache init done."));

  // LoRa initialization
  Serial.print(F("[RadioLib] Initializing ... "));
  SPI.setSCLK(LORA_SCK);
  SPI.setMISO(LORA_MISO);
  SPI.setMOSI(LORA_MOSI);
  SPI.begin();
  // 1. Инициализация с базовыми параметрами
  // Частота: 869.075, Полоса: 250.0, SF: 11, CR: 5 (Long Fast)
  int state = radio.begin(869.082f, 250.0f, 11, 5);
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
  // Периодический вывод напряжения батареи
  static unsigned long lastBatCheck = 0;
  if (millis() - lastBatCheck > 10000 || lastBatCheck == 0) {
    lastBatCheck = millis();
    float vbat = readBatteryVoltage();
    Serial.print(F("Battery Voltage: "));
    Serial.print(vbat);
    Serial.println(F(" V"));

    if (vbat < 3.5f) {
      Serial.println(F("!!! CRITICAL BATTERY VOLTAGE !!!"));
      Serial.println(F("Shutting down radio and entering deep sleep..."));
      Serial.flush();

      // Отключаем радиомодуль
      radio.sleep();

      // Отключаем светодиод
      digitalWrite(LED_PIN, LOW);

      // Полное отключение радио и уход в цикл ожидания заряда
      while (true) {
        // Спим 1 минуту (60000 мс) для экономии энергии
        LowPower.deepSleep(60000);
        
        // После просыпания проверяем напряжение
        vbat = readBatteryVoltage();
        Serial.print(F("Check voltage in shutdown: "));
        Serial.print(vbat);
        Serial.println(F(" V"));
        
        // Если напряжение поднялось выше 3.6В (небольшой гистерезис), перезагружаемся
        if (vbat > 3.6f) {
          Serial.println(F("Voltage recovered. Restarting..."));
          Serial.flush();
          HAL_NVIC_SystemReset();
        }
      }
    }
  }

  // Уходим в сон до прерывания на DIO0
  Serial.flush();
  digitalWrite(LED_PIN, LOW);
  LowPower.deepSleep(60000); //to have chance to flash # while true; do st-flash erase && break ; done

  if (digitalRead(LORA_DIO0) == HIGH) {
    digitalWrite(LED_PIN, HIGH);
    size_t len = radio.getPacketLength();
    static uint8_t buffer[256]; // Используем static для уменьшения использования стека

    // Для SX127x в RadioLib используется метод readData.
    // Флаги прерываний очищаются внутри readData автоматически.
    int state = radio.readData(buffer, len);

    if (state == RADIOLIB_ERR_NONE && len >= 16) {
        MeshHeader header;
        parseMeshHeader(buffer, &header);

        if (addPacketToCache(header.from, header.pktId)) {
            Serial.print(F("\nNew packet from 0x"));
            Serial.print(header.from, HEX);
            Serial.print(F(" with ID 0x"));
            Serial.println(header.pktId, HEX);
            
#ifdef ENABLE_PACKET_DEBUG
            printPacketInsight(buffer, len, radio, header);
#endif
        } else {
            Serial.print(F("\nDuplicate packet from 0x"));
            Serial.print(header.from, HEX);
            Serial.print(F(" with ID 0x"));
            Serial.println(header.pktId, HEX);
        }
    } else if (state == RADIOLIB_ERR_NONE) {
        Serial.println(F("Packet too short for Meshtastic header"));
    }
    
    // Очищаем прерывания и переходим в режим ожидания нового пакета
    radio.startReceive();

    // Ждем пока DIO0 упадет, чтобы не прочитать тот же пакет снова в следующем цикле loop
    // (поскольку readData очищает флаги в чипе, но пин может еще мгновение быть HIGH)
    while(digitalRead(LORA_DIO0) == HIGH) {
        delay(1);
    }
  }
}
