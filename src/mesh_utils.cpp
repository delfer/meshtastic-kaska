#include "mesh_utils.h"
#include <string.h>

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
    header->chanHash  = buffer[13];
    header->nextHop   = buffer[14];
    header->relayNode = buffer[15];
}
