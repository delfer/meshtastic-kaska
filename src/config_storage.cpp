#include "config_storage.h"
#include <EEPROM.h>

const DeviceConfig DEFAULT_CONFIG = {
    .magic = CONFIG_MAGIC,
    .version = CONFIG_VERSION,
    .radio_frequency = 869.085f,
    .radio_bandwidth = 250.0f,
    .radio_spreadingFactor = 11,
    .radio_codingRate = 5,
    .radio_syncWord = 0x2B,
    .radio_preambleLength = 16,
    .checksum = 0
};

uint16_t calculateChecksum(const DeviceConfig& cfg) {
    const uint8_t* data = (const uint8_t*)&cfg;
    uint16_t sum = 0;
    // Считаем чексумму для всех полей кроме самого поля checksum (последние 2 байта)
    for (size_t i = 0; i < sizeof(DeviceConfig) - 2; i++) {
        sum += data[i];
    }
    return sum;
}

bool loadConfig(DeviceConfig& cfg) {
    EEPROM.get(0, cfg);
    
    if (cfg.magic != CONFIG_MAGIC || cfg.version != CONFIG_VERSION) {
        Serial.println(F("Config: Magic or Version mismatch. Using defaults."));
        cfg = DEFAULT_CONFIG;
        return false;
    }
    
    uint16_t expectedChecksum = calculateChecksum(cfg);
    if (cfg.checksum != expectedChecksum) {
        Serial.println(F("Config: Checksum mismatch. Using defaults."));
        cfg = DEFAULT_CONFIG;
        return false;
    }
    
    Serial.println(F("Config: Loaded successfully from EEPROM."));
    return true;
}

void saveConfig(DeviceConfig& cfg) {
    cfg.magic = CONFIG_MAGIC;
    cfg.version = CONFIG_VERSION;
    cfg.checksum = calculateChecksum(cfg);
    EEPROM.put(0, cfg);
    Serial.println(F("Config: Saved to EEPROM."));
}
