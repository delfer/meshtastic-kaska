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

// Helper to parse Varint and advance pointer safely
uint32_t pbReadVarint(uint8_t** ptr, size_t* rem) {
    uint32_t val = 0;
    uint8_t shift = 0;
    while (*rem > 0) {
        uint8_t b = **ptr;
        (*ptr)++; (*rem)--;
        val |= (uint32_t)(b & 0x7F) << shift;
        if (!(b & 0x80)) break;
        shift += 7;
        if (shift >= 32) break; // Safety
    }
    return val;
}

// Helper to skip protobuf fields safely
void pbSkipField(uint8_t wireType, uint8_t** ptr, size_t* rem) {
    if (wireType == 0) { // Varint
        pbReadVarint(ptr, rem);
    } else if (wireType == 1) { // Fixed64
        if (*rem >= 8) { *ptr += 8; *rem -= 8; } else *rem = 0;
    } else if (wireType == 2) { // Length-delimited
        uint32_t len = pbReadVarint(ptr, rem);
        if (*rem >= len) { *ptr += len; *rem -= len; } else *rem = 0;
    } else if (wireType == 5) { // Fixed32
        if (*rem >= 4) { *ptr += 4; *rem -= 4; } else *rem = 0;
    } else {
        *rem = 0; // Abort on unknown wire type
    }
}

/**
 * @brief Печатает число с фиксированной точкой без использования float в Serial.print.
 *
 * @param value Значение в виде целого числа (scaled).
 * @param divisor Делитель для получения целой части (например, 10000000 для координат).
 * @param precision Количество знаков после запятой для вывода дробной части.
 */
void printFixedPoint(int32_t value, int32_t divisor, int8_t precision) {
    Serial.print(value / divisor);
    Serial.print('.');
    uint32_t frac = (value < 0 ? -value : value) % divisor;
    
    // Вычисляем, сколько ведущих нулей нужно добавить в дробную часть
    int32_t temp_divisor = divisor / 10;
    while (temp_divisor > 0 && frac < (uint32_t)temp_divisor && precision > 1) {
        Serial.print('0');
        temp_divisor /= 10;
        precision--;
    }
    Serial.print(frac);
}

/**
 * @brief Печатает метку с выравниванием, двоеточием и опциональным 0x.
 */
void printL(const __FlashStringHelper* label, bool hex_prefix = false) {
    Serial.print(label);
    int16_t pad = 8 - strlen_P((const char*)label);
    while (pad-- > 0) Serial.print(' ');
    Serial.print(F(": "));
    if (hex_prefix) Serial.print(F("0x"));
}

void printPacketInsight(uint8_t* buffer, size_t len, SX1276& radio) {
    Serial.println(F("\n--- [Mesh Pkt] ---"));

    if (len < 16) {
        printL(F("Pkt")); Serial.print(F("too short: ")); Serial.println(len);
        return;
    }

    // Header decoding
    uint32_t dest   = (uint32_t)buffer[0] | (uint32_t)buffer[1] << 8 | (uint32_t)buffer[2] << 16 | (uint32_t)buffer[3] << 24;
    uint32_t sender = (uint32_t)buffer[4] | (uint32_t)buffer[5] << 8 | (uint32_t)buffer[6] << 16 | (uint32_t)buffer[7] << 24;
    uint32_t packetId = (uint32_t)buffer[8] | (uint32_t)buffer[9] << 8 | (uint32_t)buffer[10] << 16 | (uint32_t)buffer[11] << 24;
    uint8_t flags   = buffer[12];
    uint8_t chanHash = buffer[13];
    uint8_t nextHop = buffer[14];
    uint8_t relayNode = buffer[15];

    printL(F("Sender"), true); Serial.println(sender, HEX);
    printL(F("Dest"), true);   Serial.print(dest, HEX);
    if (dest == 0xFFFFFFFF) Serial.println(F(" (Bcast)")); else Serial.println();
    printL(F("Pkt ID"), true); Serial.println(packetId, HEX);
    
    printL(F("Hop Lft")); Serial.println(flags & 0x07);
    printL(F("Hop Lim")); Serial.println((flags >> 5) & 0x07);
    printL(F("Wnt ACK")); Serial.println((flags >> 3) & 0x01 ? 'Y' : 'N');
    printL(F("MQTT"));    Serial.println((flags >> 4) & 0x01 ? 'Y' : 'N');

    printL(F("Chan H"), true); Serial.print(chanHash, HEX);
    if (chanHash == 0x08) {
        Serial.println(F(" (LongFast)"));
    } else if (chanHash == 0x00) {
        Serial.println(F(" (Routing/Ctrl)"));
    } else {
        Serial.println(F(" (Unknown Hash!)"));
        Serial.println(F("! Warn: Non-std hash, try key"));
    }
    printL(F("Nx Hop"), true); Serial.println(nextHop, HEX);
    printL(F("Relay"), true);  Serial.println(relayNode, HEX);

    printL(F("FreqErr")); Serial.print(radio.getFrequencyError()); Serial.println(F("Hz"));
    printL(F("Pld Size")); Serial.print(len - 16); Serial.println(F("B"));
    printL(F("RSSI/SNR")); Serial.print(radio.getRSSI()); Serial.print(F("/")); Serial.println(radio.getSNR());

    // Decryption setup
    uint8_t psk[16] = {0xd4, 0xf1, 0xbb, 0x3a, 0x20, 0x29, 0x07, 0x59,
                       0xf0, 0xbc, 0xff, 0xab, 0xcf, 0x4e, 0x69, 0x01};
    if (chanHash != 0x08 && chanHash != 0x00) {
        psk[15] = (uint8_t)(0x01 + (chanHash - 0x08));
    }

    static uint8_t payload[256]; // Переносим в static для экономии стека
    size_t payload_len = len - 16;
    if (payload_len > 256) payload_len = 256;
    memcpy(payload, buffer + 16, payload_len);
    decryptMeshtasticPayload(payload, payload_len, sender, packetId, psk, false);

    printL(F("Hex"));
    for(size_t i = 0; i < min((int)payload_len, 16); i++) {
        if(payload[i] < 0x10) Serial.print('0');
        Serial.print(payload[i], HEX); Serial.print(' ');
    }
    if (payload_len > 16) Serial.println(F("..")); else Serial.println();

    printL(F("ASCII"));
    for(size_t i = 0; i < min((int)payload_len, 32); i++) {
        uint8_t c = payload[i];
        if (c >= 32 && c <= 126) {
            Serial.print((char)c);
        } else if (c >= 0x80) {
            // Простейшая проверка на UTF-8: если это часть многобайтовой последовательности
            Serial.print((char)c);
        } else if (c == 0) {
            Serial.print(F("\\0"));
        } else {
            Serial.print('.');
        }
    }
    Serial.println();

    // Protobuf Parser (meshtastic.Data)
    uint8_t* p = payload;
    size_t rem = payload_len;
    uint32_t portNum = 0;

    while (rem > 0) {
        uint8_t* prev_outer_p = p;
        uint8_t tag = p[0];
        uint8_t wire = tag & 0x07;
        uint32_t field = tag >> 3;
        p++; rem--;

        if (field == 1 && wire == 0) { // portnum
            portNum = pbReadVarint(&p, &rem);
            printL(F("PortNum")); Serial.print(portNum);
            switch(portNum) {
                case 1:  Serial.println(F(" (TEXT)")); break;
                case 3:  Serial.println(F(" (POS)")); break;
                case 4:  Serial.println(F(" (NODEINF)")); break;
                case 67: Serial.println(F(" (TELEM)")); break;
                case 32: Serial.println(F(" (RPLY)")); break;
                case 5:  Serial.println(F(" (ROUTING)")); break;
                case 70: Serial.println(F(" (STORE_FORWARD)")); break;
                default: Serial.println(); break;
            }
        } else if (field == 2 && wire == 2) { // payload (bytes)
            uint32_t sub_len = pbReadVarint(&p, &rem);
            if (sub_len > rem) sub_len = rem; // Safety clamp
            uint8_t* sub_p = p;
            size_t sub_rem = sub_len;

            if (portNum == 1 || portNum == 32) { // TEXT or REPLY
                printL(F("Text")); Serial.print('\"');
                for(size_t i=0; i<sub_rem; i++) {
                    uint8_t c = sub_p[i];
                    if (c >= 32 && c < 127) {
                        Serial.print((char)c);
                    } else if (c >= 0x80) {
                        // UTF-8
                        Serial.print((char)c);
                    } else {
                        Serial.print('.');
                    }
                }
                Serial.println('\"');
            } else if (portNum == 3) { // POSITION
                while (sub_rem > 0) {
                    uint8_t* prev_p = sub_p;
                    uint8_t p_tag = sub_p[0];
                    uint8_t p_wire = p_tag & 0x07;
                    uint32_t p_field = p_tag >> 3;
                    sub_p++; sub_rem--;
                    if (p_field == 1 && p_wire == 5) { // lat
                        if (sub_rem >= 4) {
                            int32_t lat_i; memcpy(&lat_i, sub_p, 4);
                            printL(F("Lat"));
                            printFixedPoint(lat_i, 10000000, 7);
                            Serial.println();
                            sub_p += 4; sub_rem -= 4;
                        } else sub_rem = 0;
                    } else if (p_field == 2 && p_wire == 5) { // lon
                        if (sub_rem >= 4) {
                            int32_t lon_i; memcpy(&lon_i, sub_p, 4);
                            printL(F("Lon"));
                            printFixedPoint(lon_i, 10000000, 7);
                            Serial.println();
                            sub_p += 4; sub_rem -= 4;
                        } else sub_rem = 0;
                    } else if (p_field == 3 && p_wire == 0) { // alt
                        int32_t alt = pbReadVarint(&sub_p, &sub_rem);
                        printL(F("Alt")); Serial.print(alt); Serial.println('m');
                    } else {
                        pbSkipField(p_wire, &sub_p, &sub_rem);
                    }
                    if (sub_p == prev_p) { sub_rem = 0; break; } // Safety against infinite loop
                }
            } else if (portNum == 67) { // TELEMETRY
                while (sub_rem > 0) {
                    uint8_t* prev_p = sub_p;
                    uint8_t t_tag = sub_p[0];
                    uint8_t t_wire = t_tag & 0x07;
                    uint32_t t_field = t_tag >> 3;
                    sub_p++; sub_rem--;
                    if (t_field == 2 && t_wire == 2) { // device_metrics
                        uint32_t d_len = pbReadVarint(&sub_p, &sub_rem);
                        if (d_len > sub_rem) d_len = sub_rem;
                        uint8_t* d_p = sub_p; size_t d_rem = d_len;
                        while (d_rem > 0) {
                            uint8_t d_tag = d_p[0];
                            uint8_t d_wire = d_tag & 0x07;
                            uint32_t d_field = d_tag >> 3;
                            d_p++; d_rem--;
                            if (d_field == 1 && d_wire == 0) {
                                uint32_t bat = pbReadVarint(&d_p, &d_rem);
                                printL(F("Bat")); Serial.print(bat); Serial.println('%');
                            } else if (d_field == 2 && d_wire == 5) {
                                if (d_rem >= 4) {
                                    union { float f; uint32_t i; } conv;
                                    memcpy(&conv.i, d_p, 4);
                                    printL(F("Volt"));
                                    printFixedPoint((int32_t)(conv.f * 100), 100, 2);
                                    Serial.println('V');
                                    d_p += 4; d_rem -= 4;
                                } else d_rem = 0;
                            } else if (d_field == 3 && d_wire == 5) {
                                if (d_rem >= 4) {
                                    union { float f; uint32_t i; } conv;
                                    memcpy(&conv.i, d_p, 4);
                                    printL(F("ChUtil"));
                                    printFixedPoint((int32_t)(conv.f * 100), 100, 2);
                                    Serial.println('%');
                                    d_p += 4; d_rem -= 4;
                                } else d_rem = 0;
                            } else if (d_field == 5 && d_wire == 0) {
                                uint32_t upt = pbReadVarint(&d_p, &d_rem);
                                printL(F("Uptime")); Serial.print(upt); Serial.println('s');
                            } else {
                                pbSkipField(d_wire, &d_p, &d_rem);
                            }
                            if (d_p == prev_p) { d_rem = 0; break; } // Safety
                        }
                        sub_p += d_len; sub_rem -= d_len;
                    } else if (t_field == 3 && t_wire == 2) { // environment_metrics
                        uint8_t* prev_e_p = sub_p;
                        uint32_t e_len = pbReadVarint(&sub_p, &sub_rem);
                        if (e_len > sub_rem) e_len = sub_rem;
                        uint8_t* e_p = sub_p; size_t e_rem = e_len;
                        while (e_rem > 0) {
                            uint8_t e_tag = e_p[0];
                            uint8_t e_wire = e_tag & 0x07;
                            uint32_t e_field = e_tag >> 3;
                            e_p++; e_rem--;
                            if (e_field == 1 && e_wire == 5) {
                                if (e_rem >= 4) {
                                    union { float f; uint32_t i; } conv;
                                    memcpy(&conv.i, e_p, 4);
                                    printL(F("Temp"));
                                    printFixedPoint((int32_t)(conv.f * 100), 100, 2);
                                    Serial.println('C');
                                    e_p += 4; e_rem -= 4;
                                } else e_rem = 0;
                            } else if (e_field == 2 && e_wire == 5) {
                                if (e_rem >= 4) {
                                    union { float f; uint32_t i; } conv;
                                    memcpy(&conv.i, e_p, 4);
                                    printL(F("Humid"));
                                    printFixedPoint((int32_t)(conv.f * 100), 100, 2);
                                    Serial.println('%');
                                    e_p += 4; e_rem -= 4;
                                } else e_rem = 0;
                            } else if (e_field == 3 && e_wire == 5) {
                                if (e_rem >= 4) {
                                    union { float f; uint32_t i; } conv;
                                    memcpy(&conv.i, e_p, 4);
                                    printL(F("Pres"));
                                    printFixedPoint((int32_t)(conv.f * 100), 100, 2);
                                    Serial.println(F("hPa"));
                                    e_p += 4; e_rem -= 4;
                                } else e_rem = 0;
                            } else {
                                pbSkipField(e_wire, &e_p, &e_rem);
                            }
                            if (e_p == prev_e_p) { e_rem = 0; break; } // Safety
                        }
                        sub_p += e_len; sub_rem -= e_len;
                    } else {
                        pbSkipField(t_wire, &sub_p, &sub_rem);
                    }
                    if (sub_p == prev_p) { sub_rem = 0; break; } // Safety
                }
            }
            p += sub_len; rem -= sub_len;
        } else {
            pbSkipField(wire, &p, &rem);
        }
        if (p == prev_outer_p) { rem = 0; break; } // Safety
    }
}
