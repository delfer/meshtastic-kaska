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
    uint8_t chanHash;
    uint8_t nextHop;
    uint8_t relayNode;
};

/**
 * Извлекает заголовок из сырого буфера пакета
 */
void parseMeshHeader(const uint8_t* buffer, MeshHeader* header);

#endif // MESH_UTILS_H
