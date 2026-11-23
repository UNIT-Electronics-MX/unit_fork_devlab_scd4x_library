/**
 * @file exampleSerialNumberRaw.ino
 * @brief Lee el número de serie del SCD4x tal como lo envía el sensor.
 *
 * El sensor responde get_serial_number con 9 bytes: tres palabras de 16 bits y,
 * después de cada una, su CRC-8 (polinomio 0x31, valor inicial 0xFF):
 *
 *   [word0 msb, word0 lsb, crc0] [word1 msb, word1 lsb, crc1] [word2 msb, word2 lsb, crc2]
 *
 * El número de serie de 48 bits es word0:word1:word2.
 * Cada 5 s se imprime:
 *  - la trama cruda de 9 bytes, con el CRC recibido y el calculado de cada palabra
 *    (printSerialNumberRaw()),
 *  - el mismo dato leído con getSerialNumberRaw() y con getSerialNumber().
 * Compatible con ESP32, RP2040/RP2350, STM32 y AVR.
 *
 * @author Jonathan Mejorado
 * @organization UNIT Electronics MX
 */

#include <Arduino.h>
#include <Wire.h>
#include <DevLab_SCD4x.h>

#if defined(ARDUINO_ARCH_RP2040)
  #if defined(PIN_WIRE1_SDA) && defined(PIN_WIRE1_SCL) && \
      PIN_WIRE1_SDA == 24U && PIN_WIRE1_SCL == 25U
    constexpr uint8_t I2C_SDA = 24U, I2C_SCL = 25U;
  #else
    constexpr uint8_t I2C_SDA = 12U, I2C_SCL = 13U;
  #endif
#elif defined(ARDUINO_ARCH_ESP32)
  constexpr uint8_t I2C_SDA = 6U, I2C_SCL = 7U;
#elif defined(ARDUINO_ARCH_STM32)
  constexpr uint8_t I2C_SDA = SDA, I2C_SCL = SCL;
#elif defined(ARDUINO_ARCH_AVR)
  constexpr uint8_t I2C_SDA = SDA, I2C_SCL = SCL;
#else
  #error "Use ESP32, RP2040/RP2350, STM32 or AVR"
#endif
constexpr uint32_t I2C_CLOCK_HZ = 100000U;   // SCD4x: max. 100 kHz

DevLab_SCD4x scd(Wire, DevLab_SCD4x::DEFAULT_ADDRESS, I2C_CLOCK_HZ);

void setup() {
  Serial.begin(115200);
  delay(500);
  // false: leave the sensor idle (get_serial_number is not available while measuring)
  if (!scd.begin(I2C_SDA, I2C_SCL, I2C_CLOCK_HZ, false)) {
    Serial.print("SCD4x not found: ");
    Serial.println(DevLab_SCD4x::errorString(scd.lastError()));
  }
}

void loop() {
  if (!scd.isConnected()) return;

  Serial.println();
  scd.printSerialNumberRaw(Serial);

  uint8_t raw[9];
  bool crcOk = false;
  if (!scd.getSerialNumberRaw(raw, &crcOk)) {
    Serial.print("ERROR: ");
    Serial.println(DevLab_SCD4x::errorString(scd.lastError()));
    delay(1000U);
    return;
  }
  Serial.print("getSerialNumberRaw(): crcOk = ");
  Serial.println(crcOk ? "yes" : "no");

  uint64_t serial = 0;
  if (scd.getSerialNumber(serial)) {
    Serial.print("getSerialNumber():    0x");
    for (int shift = 44; shift >= 0; shift -= 4) {
      Serial.print((uint8_t)((serial >> shift) & 0x0F), HEX);
    }
    Serial.println();
  }
  delay(5000U);
}
