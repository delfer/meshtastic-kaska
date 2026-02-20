#ifndef UART_CONFIG_H
#define UART_CONFIG_H

#include <Arduino.h>

/**
 * @brief Инициализация обработчика UART.
 */
void uartConfigInit();

/**
 * @brief Опрос UART на наличие новых данных. Эту функцию нужно вызывать в loop().
 */
void uartConfigLoop();

#endif // UART_CONFIG_H
