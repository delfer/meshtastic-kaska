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
void decryptMeshtasticPayload(uint8_t* buffer, size_t len, uint32_t fromNode, uint32_t packetId, const uint8_t* key);

#endif // PACKET_DEBUG_H
