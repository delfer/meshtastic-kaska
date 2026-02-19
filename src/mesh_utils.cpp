#include "mesh_utils.h"
#include <string.h>
#include "tiny-aes.h"

void parseMeshHeader(const uint8_t* buffer, MeshHeader* header) {
    if (!buffer || !header) return;
    
    // Все поля заголовка передаются в Little Endian для Layer 1 Meshtastic
    // Dest: 0-3
    header->dest = (uint32_t)buffer[0] | ((uint32_t)buffer[1] << 8) | ((uint32_t)buffer[2] << 16) | ((uint32_t)buffer[3] << 24);
    // From: 4-7
    header->from = (uint32_t)buffer[4] | ((uint32_t)buffer[5] << 8) | ((uint32_t)buffer[6] << 16) | ((uint32_t)buffer[7] << 24);
    // PktID: 8-11
    header->pktId = (uint32_t)buffer[8] | ((uint32_t)buffer[9] << 8) | ((uint32_t)buffer[10] << 16) | ((uint32_t)buffer[11] << 24);
    
    header->flags     = buffer[12];
    // Разбор флагов
    header->hopStart  = header->flags & 0x07;
    header->hopLimit  = (header->flags >> 5) & 0x07;
    header->wantAck   = (header->flags >> 3) & 0x01;
    header->viaMqtt   = (header->flags >> 4) & 0x01;
    
    header->chanHash  = buffer[13];
    header->nextHop   = buffer[14];
    header->relayNode = buffer[15];
}

void initMeshtasticNonce(uint8_t *nonce, uint32_t fromNode, uint32_t packetId) {
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
    initMeshtasticNonce(nonce, fromNode, packetId);

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
    (void)is_be;
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
