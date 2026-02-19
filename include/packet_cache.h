#ifndef PACKET_CACHE_H
#define PACKET_CACHE_H

#include <Arduino.h>

/**
 * Структура для хранения идентификаторов пакета
 */
struct PacketId {
    uint32_t senderId;
    uint32_t pktId;
};

/**
 * Инициализирует кольцевой буфер.
 * Пытается выделить максимально возможный объем RAM.
 */
void packetCacheInit();

/**
 * Проверяет, есть ли такая пара SenderID + PktID в буфере.
 * @return true если пакет найден
 */
bool isPacketInCache(uint32_t senderId, uint32_t pktId);

/**
 * Добавляет пакет в кэш, если его там еще нет.
 * @return true если пакет был добавлен (его не было в кэше)
 */
bool addPacketToCache(uint32_t senderId, uint32_t pktId);

/**
 * Возвращает количество элементов в кэше.
 */
size_t getPacketCacheSize();

#endif // PACKET_CACHE_H
