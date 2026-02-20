#ifndef CONFIG_STORAGE_H
#define CONFIG_STORAGE_H

#include <Arduino.h>

#define CONFIG_MAGIC 0x4B41534B // "KASK" in hex
#define CONFIG_VERSION 1

struct DeviceConfig {
    uint32_t magic;
    uint8_t version;
    
    // Radio parameters
    float radio_frequency;
    float radio_bandwidth;
    uint8_t radio_spreadingFactor;
    uint8_t radio_codingRate;
    uint8_t radio_syncWord;
    uint16_t radio_preambleLength;
    
    uint16_t checksum;
};

// Дефолтные значения
extern const DeviceConfig DEFAULT_CONFIG;

// Объявления функций
uint16_t calculateChecksum(const DeviceConfig& cfg);
bool loadConfig(DeviceConfig& cfg);
void saveConfig(DeviceConfig& cfg);

#endif // CONFIG_STORAGE_H
