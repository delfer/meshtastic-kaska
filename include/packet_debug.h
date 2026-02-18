#ifndef PACKET_DEBUG_H
#define PACKET_DEBUG_H

#include <Arduino.h>
#include <RadioLib.h>

/**
 * @brief Выводит подробную информацию о пакете Meshtastic в консоль.
 * 
 * @param buffer Буфер с данными пакета
 * @param len Длина пакета
 * @param radio Ссылка на объект радио для получения RSSI/SNR/FreqError
 */
void printPacketInsight(uint8_t* buffer, size_t len, SX1276& radio);

#endif // PACKET_DEBUG_H
