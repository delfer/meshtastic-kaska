# meshtastic-kaska

## Wiring

### Accelerometer

LIS3DH

| Номер пина | Порт | Выбранная функция |
| ---------- | ---- | ----------------- |
| 33         | PA12 | —                 |
| 32         | PA11 | —                 |
| 26         | PB13 | I2C2_SCL          |
| 27         | PB14 | I2C2_SDA          |


### LoRa Sx1276

| **Pin Name** | **Function** | **STM32** |
| ------------ | ------------ | --------- |
| **GND**      | Ground       | GND       |
| **SCK**      | SPI Clock    | 15 PA5    |
| **MISO**     | SPI Data Out | 16 PA6    |
| **MOSI**     | SPI Data In  | 17 PA7    |
| **NSS**      | Chip Select  | 14 PA4    |
| **DIO2**     | Digital I/O  | 18 PB0    |
| **DIO1**     |              | 19 PB1    |
| **DIO0**     |              | 20 PB2    |
| **VCC**      | Power        | 3V3       |
| **NRESET**   | Reset        | 28 PB15   |
