#include <Arduino.h>
#include <Wire.h>

#define LED_PIN PA15
#define I2C2_SCL PB13
#define I2C2_SDA PB14

TwoWire Wire2(I2C2_SDA, I2C2_SCL);

uint8_t readRegister(uint8_t address, uint8_t reg) {
  Wire2.beginTransmission(address);
  Wire2.write(reg);
  if (Wire2.endTransmission(false) != 0) return 0xFF;
  
  Wire2.requestFrom(address, (uint8_t)1);
  if (Wire2.available()) return Wire2.read();
  return 0xFF;
}

void scanI2C() {
  byte error, address;
  int nDevices;

  Serial.println("Scanning I2C2 (PB13/PB14)...");

  nDevices = 0;
  for (address = 1; address < 127; address++) {
    Wire2.beginTransmission(address);
    error = Wire2.endTransmission();

    if (error == 0) {
      Serial.print("I2C device found at address 0x");
      if (address < 16)
        Serial.print("0");
      Serial.print(address, HEX);
      
      // Common WHO_AM_I registers: 0x0F (most common), 0x00, 0x75 (MPU/ICM)
      uint8_t whoAmI = readRegister(address, 0x0F);
      if (whoAmI != 0xFF) {
        Serial.print(" [WHO_AM_I(0x0F): 0x");
        Serial.print(whoAmI, HEX);
        Serial.print("]");
        
        // Known devices lookup
        if (address == 0x19) {
          if (whoAmI == 0x33) Serial.print(" -> LIS3DH");
          else if (whoAmI == 0x41) Serial.print(" -> LSM303AGR");
        } else if (address == 0x3C || address == 0x3D) {
          Serial.print(" -> SSD1306 Display?");
        } else if (address == 0x68 || address == 0x69) {
          if (whoAmI == 0x68) Serial.print(" -> MPU6050");
          else if (whoAmI == 0x71) Serial.print(" -> MPU9250");
        }
      }
      
      Serial.println("  !");

      nDevices++;
    } else if (error == 4) {
      Serial.print("Unknown error at address 0x");
      if (address < 16)
        Serial.print("0");
      Serial.println(address, HEX);
    }
  }
  if (nDevices == 0)
    Serial.println("No I2C devices found\n");
  else
    Serial.println("done\n");
}

void setup() {
  Serial.setTx(PA9);
  Serial.setRx(PA10);
  Serial.begin(115200);
  
  pinMode(LED_PIN, OUTPUT);
  Serial.println("Debug output initialized on PA9");

  Wire2.begin();
}

void loop() {
  digitalWrite(LED_PIN, HIGH);
  Serial.println("LED ON");
  delay(200);
  scanI2C();
  digitalWrite(LED_PIN, LOW);
  Serial.println("LED OFF");
  delay(10000);
}
