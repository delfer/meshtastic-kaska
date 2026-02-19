#include "packet_debug.h"

// Используем локальную реализацию tiny-aes, настроенную на AES-128
#include "tiny-aes.h"

void initMeshtasticNonce(uint8_t *nonce, uint32_t fromNode, uint32_t packetId, bool is_be) {
    memset(nonce, 0, 16);
    // Nonce (16 bytes) для Meshtastic V2:
    // 1. Packet ID (8 байт, Little Endian)
    // 2. Sender Node ID (4 байта, Little Endian)
    // 3. Block Counter (4 байта, Big Endian - заполняется в decryptMeshtasticCTR)
    
    uint64_t pid64 = packetId;
    nonce[0] = (pid64 >> 0) & 0xFF;
    nonce[1] = (pid64 >> 8) & 0xFF;
    nonce[2] = (pid64 >> 16) & 0xFF;
    nonce[3] = (pid64 >> 24) & 0xFF;
    nonce[4] = (pid64 >> 32) & 0xFF;
    nonce[5] = (pid64 >> 40) & 0xFF;
    nonce[6] = (pid64 >> 48) & 0xFF;
    nonce[7] = (pid64 >> 56) & 0xFF;

    nonce[8] = (fromNode >> 0) & 0xFF;
    nonce[9] = (fromNode >> 8) & 0xFF;
    nonce[10] = (fromNode >> 16) & 0xFF;
    nonce[11] = (fromNode >> 24) & 0xFF;
}

// Кастомная реализация CTR для Meshtastic
void decryptMeshtasticCTR(uint8_t* buffer, size_t len, uint32_t fromNode, uint32_t packetId, const uint8_t* key) {
    uint8_t nonce[16];
    initMeshtasticNonce(nonce, fromNode, packetId, false); // is_be игнорируем, используем стандарт V2

    struct AES_ctx ctx;
    AES_init_ctx(&ctx, key);

    uint8_t stream_block[16];
    uint32_t block_count = 0;

    for (size_t i = 0; i < len; i++) {
        if ((i & 0x0F) == 0) {
            uint8_t counter_block[16];
            memcpy(counter_block, nonce, 16);
            
            // Block Counter (байты 12-15) в Big Endian для CTR
            counter_block[15] = (block_count >> 0) & 0xFF;
            counter_block[14] = (block_count >> 8) & 0xFF;
            counter_block[13] = (block_count >> 16) & 0xFF;
            counter_block[12] = (block_count >> 24) & 0xFF;
            
            memcpy(stream_block, counter_block, 16);
            Cipher((state_t*)stream_block, ctx.RoundKey);
            block_count++;
        }
        buffer[i] ^= stream_block[i & 0x0F];
    }
}

void decryptMeshtasticPayload(uint8_t* buffer, size_t len, uint32_t fromNode, uint32_t packetId, const uint8_t* key, bool is_be) {
    decryptMeshtasticCTR(buffer, len, fromNode, packetId, key);
}

void printPacketInsight(uint8_t* buffer, size_t len, SX1276& radio) {
    Serial.println(F("\n--- [Meshtastic Packet] ---"));

    if (len < 16) {
        Serial.print(F("Packet too short: ")); Serial.println(len);
        return;
    }

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
        // Расшифровка
        // Meshtastic использует специальный ключ для публичных каналов.
        // Расшифровка
        // Meshtastic использует специальный ключ для публичных каналов.
        // Ключ "AQ==" (0x01) является "алиасом" для стандартного 128-битного ключа.
        // В Meshtastic используется AES-128 для 1-байтовых ключей.
        // Стандартный ключ defaultpsk для Meshtastic (LongFast)
        // Стандартный ключ defaultpsk для Meshtastic (LongFast)
        uint8_t psk[16] = {0xd4, 0xf1, 0xbb, 0x3a, 0x20, 0x29, 0x07, 0x59,
                           0xf0, 0xbc, 0xff, 0xab, 0xcf, 0x4e, 0x69, 0x01};
        
        // По хешу канала можно понять, какой ключ использовать.
        // 0x08 - это хеш для LongFast (defaultpsk с индексом 1)
        // Если Chan Hash != 0x08, пробуем подобрать ключ на основе разницы хешей.
        if (chanHash != 0x08 && chanHash != 0x00) {
            // В Meshtastic хеш канала зависит и от имени, и от PSK.
            // Но часто используются стандартные индексы PSK (1, 2, 3...).
            // Если это стандартный канал (напр. MediumFast), psk[15] может отличаться.
            // psk[15] = 0x01 + (pskIndex - 1)
            // Попробуем просто применить смещение хеша к байту ключа (очень упрощенно).
            psk[15] = (uint8_t)(0x01 + (chanHash - 0x08));
        }

        // Копируем полезную нагрузку для расшифровки
        uint8_t encrypted_payload[256];
        size_t payload_len = len - 16;
        if (payload_len > 256) payload_len = 256;
        
        // --- РАСШИФРОВКА ---
        uint8_t current_psk[16]; memcpy(current_psk, psk, 16);
        // Если Chan Hash отличается от стандартного LongFast (0x08), пробуем адаптировать ключ
        if (chanHash != 0x08 && chanHash != 0x00) {
            current_psk[15] = (uint8_t)(0x01 + (chanHash - 0x08));
        }

        memcpy(encrypted_payload, buffer + 16, payload_len);
        decryptMeshtasticPayload(encrypted_payload, payload_len, sender, packetId, current_psk, false);

        Serial.print(F("Selected Nonce: "));
        uint8_t debug_nonce[16];
        initMeshtasticNonce(debug_nonce, sender, packetId, false);
        for(int i=0; i<16; i++) {
            if(debug_nonce[i] < 0x10) Serial.print('0');
            Serial.print(debug_nonce[i], HEX); Serial.print(' ');
        }
        Serial.println();

        Serial.print(F("Decrypted Hex:  "));
        for(size_t i = 0; i < min((int)payload_len, 16); i++) {
            if(encrypted_payload[i] < 0x10) Serial.print('0');
            Serial.print(encrypted_payload[i], HEX); Serial.print(' ');
        }
        if (payload_len > 16) Serial.println(F("...")); else Serial.println();

        // Попытка интерпретировать как текст (PortNum_TEXT_MESSAGE_APP = 1)
        // В Meshtastic внутри зашифрованного payload обычно находится Protobuf meshtastic.Data
        // Но для начала выведем просто как строку, если там есть читаемые символы
        Serial.print(F("Decrypted ASCII: "));
        for(size_t i = 0; i < min((int)payload_len, 32); i++) {
            char c = encrypted_payload[i];
            if (c >= 32 && c <= 126) Serial.print(c);
            else if (c == 0) Serial.print(F("\\0"));
            else Serial.print('.');
        }
        Serial.println();

        // Попытка найти текст внутри meshtastic.Data (Protobuf)
        // meshtastic.Data { string payload = 2; ... } -> tag 2, wire type 2 (length-delimited) -> 0x12
        // Но сначала может идти PortNum (tag 1, varint).
        // Если это TEXT_MESSAGE_APP (1), то байты будут 0x08 0x01
        
        uint8_t* p = encrypted_payload;
        size_t rem = payload_len;

        uint32_t portNum = 0;
        bool foundPortNum = false;

        // Meshtastic Data (Protobuf) simple parser
        // Tag 1: PortNum (Varint)
        // Tag 2: Payload (Length-delimited)
        while (rem > 0) {
            uint8_t tag = p[0];
            uint8_t wireType = tag & 0x07;
            uint32_t fieldNum = tag >> 3;

            if (fieldNum == 1 && wireType == 0) { // PortNum (Varint)
                p++; rem--;
                uint32_t val = 0;
                uint8_t shift = 0;
                while(rem > 0) {
                    uint8_t b = p[0];
                    val |= (uint32_t)(b & 0x7F) << shift;
                    p++; rem--;
                    if (!(b & 0x80)) break;
                    shift += 7;
                }
                portNum = val;
                foundPortNum = true;
                
                Serial.print(F("PortNum:      ")); Serial.print(portNum);
                switch(portNum) {
                    case 1:  Serial.println(F(" (TEXT_MESSAGE)")); break;
                    case 2:  Serial.println(F(" (REMOTE_HARDWARE)")); break;
                    case 3:  Serial.println(F(" (POSITION)")); break;
                    case 4:  Serial.println(F(" (NODEINFO)")); break;
                    case 5:  Serial.println(F(" (ROUTING)")); break;
                    case 6:  Serial.println(F(" (ADMIN)")); break;
                    case 9:  Serial.println(F(" (AUDIO)")); break;
                    case 32: Serial.println(F(" (REPLY)")); break;
                    case 67: Serial.println(F(" (TELEMETRY)")); break;
                    case 70: Serial.println(F(" (TRACEROUTE)")); break;
                    case 71: Serial.println(F(" (NEIGHBORINFO)")); break;
                    default: Serial.println(); break;
                }
            } else if (fieldNum == 2 && wireType == 2) { // Payload (bytes/string)
                uint8_t pb_len = p[1];
                if (pb_len <= rem - 2) {
                    if (portNum == 1 || portNum == 32) { // Text or Reply
                        Serial.print(F("Probable Text: \""));
                        for(int i=0; i<pb_len; i++) {
                            char c = p[i+2];
                            if (c >= 32 && c < 127) Serial.print(c);
                            else Serial.print('.');
                        }
                        Serial.println('\"');
                    }
                }
                // Skip length-delimited
                p += 2 + pb_len;
                if (rem >= (size_t)(2 + pb_len)) rem -= (2 + pb_len); else rem = 0;
            } else {
                // Skip unknown tags
                if (wireType == 0) { // Varint
                    p++; rem--;
                    while(rem > 0 && (p[0] & 0x80)) { p++; rem--; }
                    p++; rem--;
                } else if (wireType == 2) { // Length-delimited
                    uint8_t l = p[1];
                    p += 2 + l;
                    if (rem >= (size_t)(2 + l)) rem -= (2 + l); else rem = 0;
                } else {
                    break;
                }
            }
        }
    }

    Serial.print(F("RSSI/SNR:    ")); Serial.print(radio.getRSSI()); 
    Serial.print(F(" / ")); Serial.println(radio.getSNR());
}
