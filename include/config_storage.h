#ifndef CONFIG_STORAGE_H
#define CONFIG_STORAGE_H

#include <Arduino.h>

#define CONFIG_MAGIC 0x4B41534B // "KASK" in hex
#define CONFIG_VERSION 4

#pragma pack(push, 1)
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

    // Power and ADC parameters
    float adc_multiplier;
    float battery_threshold;

    // Security parameters
    uint8_t aes_key[16];

    // Logging level: 0 - none, 1 - packets, 2 - insight
    uint8_t log_level;
    
    // Relay delay: -1 = disabled, 0+ = delay in ms (packet is retransmitted without modification) (UART command: dlrl)
    int32_t relay_delay;
    
    uint16_t checksum;
};
#pragma pack(pop)

// Дефолтные значения
extern const DeviceConfig DEFAULT_CONFIG;

extern DeviceConfig currentConfig;

// Объявления функций
uint16_t calculateChecksum(const DeviceConfig& cfg);
bool loadConfig(DeviceConfig& cfg);
void saveConfig(DeviceConfig& cfg);

#endif // CONFIG_STORAGE_H
