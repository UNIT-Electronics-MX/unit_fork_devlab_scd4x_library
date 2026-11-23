/**
 * @file exampleScd41SingleShot.ino
 * @brief Medición bajo demanda (single shot) con DevLab_SCD4x. Solo SCD41/SCD43.
 *
 * El sensor se deja en reposo (begin(..., false)) y cada medición tarda ~5 s.
 * Entre mediciones se apaga con powerDown() para ahorrar energía.
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
constexpr uint32_t I2C_CLOCK_HZ = 100000U;
constexpr uint32_t SLEEP_MS = 30000U;

DevLab_SCD4x scd(Wire, DevLab_SCD4x::DEFAULT_ADDRESS, I2C_CLOCK_HZ);

void setup() {
  Serial.begin(115200);
  delay(500);
  if (!scd.begin(I2C_SDA, I2C_SCL, I2C_CLOCK_HZ, false)) {   // false = no periodic mode
    Serial.print("SCD4x not found: ");
    Serial.println(DevLab_SCD4x::errorString(scd.lastError()));
    return;
  }
  scd.printInfo(Serial);
}

void loop() {
  if (!scd.isConnected()) return;

  DevLab_SCD4x::Measurement m;
  if (!scd.measureSingleShot(m)) {
    Serial.print("ERROR: ");
    Serial.println(DevLab_SCD4x::errorString(scd.lastError()));
    delay(1000U);
    return;
  }
  Serial.print("CO2: ");
  Serial.print(m.co2);
  Serial.print(" ppm  T: ");
  Serial.print(m.temperatureC, 2);
  Serial.print(" C  RH: ");
  Serial.print(m.humidity, 2);
  Serial.println(" %");

  scd.powerDown();
  Serial.print("sleep for ");
  Serial.print(SLEEP_MS / 1000U);
  Serial.println(" s");
  delay(SLEEP_MS);
  scd.wakeUp();
}
