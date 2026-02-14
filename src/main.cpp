#include <Arduino.h>

/*
 * Проблема: digitalWrite() и pinMode() не работали ожидаемым образом.
 * Причины:
 * 1. pinMode(LED_PIN, PA_9) - была синтаксическая ошибка (перепутан режим и пин).
 * 2. Даже после исправления, digitalWrite не менял ODR. Это часто случается на
 *    кастомных платах (как rhf76_052), если маппинг пинов в Arduino Core
 *    не совпадает с реальной разводкой или если пины PA15/PA9 зарезервированы
 *    системой (например, под отладку или USB).
 *
 * Решение: Использование прямой работы с регистрами через STM32 HAL.
 */

#define LED_PIN PA_15

void setup() {
  // 1. Включаем тактирование порта A
  __HAL_RCC_GPIOA_CLK_ENABLE();

  // 2. Инициализируем пины через HAL для обхода ограничений Arduino Core
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Pin = GPIO_PIN_15 | GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

void loop() {
  // Включаем светодиоды (Высокий уровень)
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15 | GPIO_PIN_9, GPIO_PIN_SET);
  delay(1000);

  // Выключаем светодиоды (Низкий уровень)
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15 | GPIO_PIN_9, GPIO_PIN_RESET);
  delay(1000);
}
