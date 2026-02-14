#ifndef I2C_SCANNER_H
#define I2C_SCANNER_H

#include <Arduino.h>
#include <Wire.h>

#define I2C2_SCL PB13
#define I2C2_SDA PB14

extern TwoWire Wire2;

uint8_t readRegister(uint8_t address, uint8_t reg);
void scanI2C();

#endif // I2C_SCANNER_H
