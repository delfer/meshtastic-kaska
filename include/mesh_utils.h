#ifndef MESH_UTILS_H
#define MESH_UTILS_H

#include <Arduino.h>

/**
 * Структура заголовка Meshtastic (Layer 1)
 */
struct MeshHeader {
    uint32_t dest;
    uint32_t from;
    uint32_t pktId;
    uint8_t flags;
    uint8_t hopStart;
    uint8_t hopLimit;
    bool wantAck;
    bool viaMqtt;
    uint8_t chanHash;
    uint8_t nextHop;
    uint8_t relayNode;
};

/**
 * Извлекает заголовок из сырого буфера пакета
 */
void parseMeshHeader(const uint8_t* buffer, MeshHeader* header);

/**
 * @brief Инициализирует 128-битный nonce для расшифровки.
 *
 * @param nonce Выходной буфер (16 байт)
 * @param fromNode ID отправителя
 * @param packetId ID пакета
 */
void initMeshtasticNonce(uint8_t *nonce, uint32_t fromNode, uint32_t packetId);

/**
 * @brief Расшифровывает Payload Meshtastic (AES-CTR).
 *
 * @param buffer Указатель на начало зашифрованных данных в пакете
 * @param len Длина зашифрованных данных
 * @param fromNode ID отправителя
 * @param packetId ID пакета
 * @param key Ключ шифрования (128 бит)
 */
void decryptMeshtasticPayload(uint8_t* buffer, size_t len, uint32_t fromNode, uint32_t packetId, const uint8_t* key, bool is_be = false);

/**
 * @brief Читает Protobuf Varint и сдвигает указатель.
 */
uint32_t pbReadVarint(uint8_t** ptr, size_t* rem);

/**
 * @brief Пропускает поле Protobuf.
 */
void pbSkipField(uint8_t wireType, uint8_t** ptr, size_t* rem);

#endif // MESH_UTILS_H
