#include "packet_cache.h"
#include <stdlib.h>

// Указатель на массив структур в RAM
static PacketId* cache = NULL;
static size_t cacheCapacity = 0;
static size_t currentIndex = 0;
static size_t currentSize = 0;

// Оставляем небольшой запас RAM для работы стека и других нужд (в байтах)
#define RAM_RESERVE 2048

void packetCacheInit() {
    // В STM32L051C8 8 КБ RAM. Из логов видно: used 2508 bytes from 8192 bytes.
    // Свободно около 5684 байт.
    // Попробуем выделить максимально возможный кусок, уменьшая размер, пока malloc не сработает.
    
    size_t testSize = (8192 - 2508 - RAM_RESERVE) / sizeof(PacketId);
    
    Serial.print(F("Starting cache allocation, target slots: "));
    Serial.println(testSize);

    while (testSize > 0) {
        size_t bytesToAlloc = testSize * sizeof(PacketId);
        cache = (PacketId*)malloc(bytesToAlloc);
        if (cache != NULL) {
            cacheCapacity = testSize;
            memset(cache, 0, bytesToAlloc); // Обнуляем память, чтобы не ловить мусор
            Serial.print(F("Allocated "));
            Serial.print(bytesToAlloc);
            Serial.println(F(" bytes."));
            break;
        }
        testSize -= 10;
        if (testSize < 10 && testSize > 0) testSize = 0;
    }

    if (cache != NULL) {
        Serial.print(F("Packet cache initialized with "));
        Serial.print(cacheCapacity);
        Serial.println(F(" slots."));
    } else {
        Serial.println(F("Failed to initialize packet cache!"));
    }
}

bool isPacketInCache(uint32_t senderId, uint32_t pktId) {
    if (cache == NULL) return false;

    for (size_t i = 0; i < currentSize; i++) {
        if (cache[i].senderId == senderId && cache[i].pktId == pktId) {
            return true;
        }
    }
    return false;
}

bool addPacketToCache(uint32_t senderId, uint32_t pktId) {
    if (cache == NULL || cacheCapacity == 0) return false;

    if (isPacketInCache(senderId, pktId)) {
        return false;
    }

    // Добавляем в кольцевой буфер
    cache[currentIndex].senderId = senderId;
    cache[currentIndex].pktId = pktId;

    currentIndex = (currentIndex + 1) % cacheCapacity;
    if (currentSize < cacheCapacity) {
        currentSize++;
    }

    return true;
}

size_t getPacketCacheSize() {
    return currentSize;
}
