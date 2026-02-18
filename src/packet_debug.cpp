#include "packet_debug.h"

void printPacketInsight(uint8_t* buffer, size_t len, SX1276& radio) {
    Serial.println(F("\n--- [Meshtastic Packet] ---"));

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
}
