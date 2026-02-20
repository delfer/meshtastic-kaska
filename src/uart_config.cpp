#include "uart_config.h"
#include "config_storage.h"
#include "packet_debug.h"

/**
 * @brief Простой парсер float для экономии места.
 * Работает только с положительными числами.
 */
static float fast_atof(const char* s) {
    float rez = 0, fact = 1;
    if (*s == '-') { s++; fact = -1; };
    for (int point_seen = 0; *s; s++) {
        if (*s == '.') { point_seen = 1; continue; };
        int d = *s - '0';
        if (d >= 0 && d <= 9) {
            if (point_seen) fact /= 10.0f;
            rez = rez * 10.0f + (float)d;
        };
    };
    return rez * fact;
};

#define MAX_CMD_LEN 64
static char inputBuffer[MAX_CMD_LEN];
static uint8_t bufferIdx = 0;

void processCommand(char* cmd) {
    // Формат: <key>=<value>
    char* separator = strchr(cmd, '=');
    
    if (separator) {
        *separator = '\0';
        char* key = cmd;
        char* val = separator + 1;

        if (strcmp(key, "freq") == 0) {
            currentConfig.radio_frequency = fast_atof(val);
        } else if (strcmp(key, "sf") == 0) {
            currentConfig.radio_spreadingFactor = (uint8_t)strtol(val, NULL, 10);
        } else if (strcmp(key, "bw") == 0) {
            currentConfig.radio_bandwidth = fast_atof(val);
        } else if (strcmp(key, "cr") == 0) {
            currentConfig.radio_codingRate = (uint8_t)strtol(val, NULL, 10);
        } else if (strcmp(key, "sw") == 0) {
            currentConfig.radio_syncWord = (uint8_t)strtol(val, NULL, 0);
        } else if (strcmp(key, "pre") == 0) {
            currentConfig.radio_preambleLength = (uint16_t)strtol(val, NULL, 10);
        } else if (strcmp(key, "adc") == 0) {
            currentConfig.adc_multiplier = fast_atof(val);
        } else if (strcmp(key, "batt") == 0) {
            currentConfig.battery_threshold = fast_atof(val);
        } else if (strcmp(key, "key") == 0) {
            // Ожидаем HEX строку из 32 символов (16 байт)
            if (strlen(val) == 32) {
                for (size_t i = 0; i < 16; i++) {
                    char tmp[3] = {val[i*2], val[i*2+1], '\0'};
                    currentConfig.aes_key[i] = (uint8_t)strtol(tmp, NULL, 16);
                }
            }
        }
        Serial.print(F("Set ")); Serial.print(key);
        Serial.print(F("="));
        
        // Выводим значение без использования Serial.print(float)
        if (strcmp(key, "freq") == 0) {
            printFixedPoint((int32_t)(currentConfig.radio_frequency * 1000), 1000, 3);
        } else if (strcmp(key, "bw") == 0) {
            printFixedPoint((int32_t)(currentConfig.radio_bandwidth * 10), 10, 1);
        } else if (strcmp(key, "adc") == 0) {
            printFixedPoint((int32_t)(currentConfig.adc_multiplier * 1000000), 1000000, 6);
        } else if (strcmp(key, "batt") == 0) {
            printFixedPoint((int32_t)(currentConfig.battery_threshold * 100), 100, 2);
        } else if (strcmp(key, "key") == 0) {
            Serial.print(F("REDACTED"));
        } else {
            Serial.print(val);
        }

        Serial.println(F(" OK"));
    } else {
        // Одиночные команды (чтение значения по ключу или системные команды)
        bool handled = false;
        const char* key = cmd;
        
        if (strcmp(key, "freq") == 0) {
            Serial.print(key); Serial.print(F("="));
            printFixedPoint((int32_t)(currentConfig.radio_frequency * 1000), 1000, 3);
            handled = true;
        } else if (strcmp(key, "sf") == 0) {
            Serial.print(key); Serial.print(F("="));
            Serial.print(currentConfig.radio_spreadingFactor);
            handled = true;
        } else if (strcmp(key, "bw") == 0) {
            Serial.print(key); Serial.print(F("="));
            printFixedPoint((int32_t)(currentConfig.radio_bandwidth * 10), 10, 1);
            handled = true;
        } else if (strcmp(key, "cr") == 0) {
            Serial.print(key); Serial.print(F("="));
            Serial.print(currentConfig.radio_codingRate);
            handled = true;
        } else if (strcmp(key, "sw") == 0) {
            Serial.print(key); Serial.print(F("="));
            Serial.print(currentConfig.radio_syncWord, HEX);
            handled = true;
        } else if (strcmp(key, "pre") == 0) {
            Serial.print(key); Serial.print(F("="));
            Serial.print(currentConfig.radio_preambleLength);
            handled = true;
        } else if (strcmp(key, "adc") == 0) {
            Serial.print(key); Serial.print(F("="));
            printFixedPoint((int32_t)(currentConfig.adc_multiplier * 1000000), 1000000, 6);
            handled = true;
        } else if (strcmp(key, "batt") == 0) {
            Serial.print(key); Serial.print(F("="));
            printFixedPoint((int32_t)(currentConfig.battery_threshold * 100), 100, 2);
            handled = true;
        } else if (strcmp(key, "key") == 0) {
            Serial.print(key); Serial.print(F("=REDACTED"));
            handled = true;
        }

        if (handled) {
            Serial.println();
        } else {
            if (strcmp(cmd, "apply") == 0) {
                Serial.println(F("Saving & Rebooting..."));
                saveConfig(currentConfig);
                delay(500);
                NVIC_SystemReset();
            }
        }
    }
}

void uartConfigLoop() {
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
            if (bufferIdx > 0) {
                inputBuffer[bufferIdx] = '\0';
                processCommand(inputBuffer);
                bufferIdx = 0;
            }
        } else if (bufferIdx < MAX_CMD_LEN - 1) {
            inputBuffer[bufferIdx++] = c;
        }
    }
}
