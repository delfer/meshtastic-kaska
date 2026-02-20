#ifndef PACKET_DEBUG_H
#define PACKET_DEBUG_H

#include <Arduino.h>
#include <RadioLib.h>
#include "mesh_utils.h"

/**
 * @brief Выводит подробную информацию о пакете Meshtastic в консоль.
 *
 * @param buffer Буфер с данными пакета
 * @param len Длина пакета
 * @param radio Ссылка на объект радио для получения RSSI/SNR/FreqError
 * @param header Распарсенный заголовок пакета (должен быть заполнен)
 */
void printPacketInsight(uint8_t* buffer, size_t len, SX1276& radio, const MeshHeader& header);

/**
 * @brief Печатает число с фиксированной точкой без использования float в Serial.print.
 *
 * @param value Значение в виде целого числа (scaled).
 * @param divisor Делитель для получения целой части (например, 10000000 для координат).
 * @param precision Количество знаков после запятой для вывода дробной части.
 */
void printFixedPoint(int32_t value, int32_t divisor, int8_t precision);

#endif // PACKET_DEBUG_H
