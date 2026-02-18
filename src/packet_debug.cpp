#include "packet_debug.h"

// Используем реализацию tiny-aes из прошивки meshtastic
#include "../meshtastic-firmware/src/platform/nrf52/aes-256/tiny-aes.cpp"

void initMeshtasticNonce(uint8_t *nonce, uint32_t fromNode, uint32_t packetId) {
    memset(nonce, 0, 16);
    // NONCE: 64-bit packetId (LE), 32-bit fromNode (LE), 32-bit block counter (0)
    memcpy(nonce, &packetId, sizeof(uint32_t)); // Meshtastic packetId is 32-bit in many places but stored in 64-bit slot
    // packetId в Meshtastic (uint32_t) записывается в первые 8 байт (LE)
    // fromNode записывается в следующие 4 байта
    memcpy(nonce + 8, &fromNode, sizeof(uint32_t));
}

void decryptMeshtasticPayload(uint8_t* buffer, size_t len, uint32_t fromNode, uint32_t packetId, const uint8_t* key) {
    uint8_t nonce[16];
    initMeshtasticNonce(nonce, fromNode, packetId);

    struct AES_ctx ctx;
    AES_init_ctx_iv(&ctx, key, nonce);
    AES_CTR_xcrypt_buffer(&ctx, buffer, len);
}

void printPacketInsight(uint8_t* buffer, size_t len, SX1276& radio) {
    Serial.println(F("\n--- [Meshtastic Packet] ---"));

    if (len >= 16) {
        // Offset 0x00: Destination (4 bytes, Little-endian) - Unique NodeID of the recipient
        uint32_t dest   = (uint32_t)buffer[0] | (uint32_t)buffer[1] << 8 | (uint32_t)buffer[2] << 16 | (uint32_t)buffer[3] << 24;
        // Offset 0x04: Sender (4 bytes, Little-endian) - Unique NodeID of the sender
        uint32_t sender = (uint32_t)buffer[4] | (uint32_t)buffer[5] << 8 | (uint32_t)buffer[6] << 16 | (uint32_t)buffer[7] << 24;
        // Offset 0x08: Packet ID (4 bytes, Little-endian) - Unique ID for this packet from this sender
        uint32_t packetId = (uint32_t)buffer[8] | (uint32_t)buffer[9] << 8 | (uint32_t)buffer[10] << 16 | (uint32_t)buffer[11] << 24;
        
        // Offset 0x0C: Flags (1 byte)
        uint8_t flags   = buffer[12];
        uint8_t hopLimit = flags & 0x07;           // Bits 0-2: Remaining number of hops
        bool wantAck     = (flags >> 3) & 0x01;    // Bit 3: Whether the sender wants an acknowledgement
        bool viaMqtt     = (flags >> 4) & 0x01;    // Bit 4: Set if packet was sent via MQTT
        uint8_t hopStart = (flags >> 5) & 0x07;    // Bits 5-7: Original hop limit when packet was sent

        // Offset 0x0D: Channel hash (1 byte) - Used by receiver to identify the correct decryption key
        uint8_t chanHash = buffer[13];

        // Offset 0x0E: Next-hop (1 byte) - Used for source routing
        uint8_t nextHop = buffer[14];

        // Offset 0x0F: Relay node (1 byte) - The NodeID of the node that actually transmitted this packet
        uint8_t relayNode = buffer[15];

        Serial.print(F("Sender ID:    0x")); Serial.println(sender, HEX);
        Serial.print(F("Dest ID:      0x")); Serial.print(dest, HEX);
        if (dest == 0xFFFFFFFF) Serial.println(F(" (Broadcast)")); else Serial.println();
        
        Serial.print(F("Packet ID:    0x")); Serial.println(packetId, HEX);

        Serial.print(F("Hop Limit:    ")); Serial.println(hopLimit);
        Serial.print(F("Hop Start:    ")); Serial.println(hopStart);
        Serial.print(F("Want ACK:     ")); Serial.println(wantAck ? F("Yes") : F("No"));
        Serial.print(F("Via MQTT:     ")); Serial.println(viaMqtt ? F("Yes") : F("No"));
        Serial.print(F("Chan Hash:    0x")); Serial.println(chanHash, HEX);
        Serial.print(F("Next Hop:     0x")); Serial.println(nextHop, HEX);
        Serial.print(F("Relay Node:   0x")); Serial.println(relayNode, HEX);

        // Отладочные данные железа
        Serial.print(F("Freq Error:   ")); Serial.print(radio.getFrequencyError()); Serial.println(F(" Hz"));
        Serial.print(F("Payload Size: ")); Serial.print(len - 16); Serial.println(F(" bytes"));

        // Расшифровка
        uint8_t psk_default[32] = {0};
        psk_default[0] = 0x01; // Ключ "AQ==" -> 0x01
        
        // Копируем полезную нагрузку для расшифровки, чтобы не портить оригинал если нужно
        uint8_t encrypted_payload[256];
        size_t payload_len = len - 16;
        if (payload_len > 256) payload_len = 256;
        memcpy(encrypted_payload, buffer + 16, payload_len);

        decryptMeshtasticPayload(encrypted_payload, payload_len, sender, packetId, psk_default);

        Serial.print(F("Decrypted Hex: "));
        for(size_t i = 0; i < min((int)payload_len, 16); i++) {
            if(encrypted_payload[i] < 0x10) Serial.print('0');
            Serial.print(encrypted_payload[i], HEX);
            Serial.print(' ');
        }
        if (payload_len > 16) Serial.println(F("...")); else Serial.println();

        // Попытка интерпретировать как текст (PortNum_TEXT_MESSAGE_APP = 1)
        // В Meshtastic внутри зашифрованного payload обычно находится Protobuf meshtastic.Data
        // Но для начала выведем просто как строку, если там есть читаемые символы
        Serial.print(F("Decrypted ASCII: "));
        for(size_t i = 0; i < min((int)payload_len, 32); i++) {
            char c = encrypted_payload[i];
            if (c >= 32 && c <= 126) Serial.print(c);
            else Serial.print('.');
        }
        Serial.println();
    }

    Serial.print(F("RSSI/SNR:    ")); Serial.print(radio.getRSSI()); 
    Serial.print(F(" / ")); Serial.println(radio.getSNR());
}
