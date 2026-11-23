/*
  DevLab_SCD4x.h

  DevLab class for the Sensirion SCD4x CO2 / temperature / humidity sensor
  family (SCD40, SCD41, SCD42, SCD43).

  All the I2C traffic goes through DevLab_I2C_Interface (DevLab_Interface
  library >= 1.3.0, writeRaw()/readRaw()). The SCD4x speaks a command protocol instead of a register map:
  16-bit commands, 16-bit data words each followed by a CRC-8, and a
  command-dependent execution time between the command and its response.
  That protocol layer is private to this class, so the user only creates the
  object, calls begin() and reads measurements.

  Implemented on top of the DevLab bus only; there are no other dependencies.

  Organization:
  UNIT Electronics - DevLab Ecosystem

  Author:
  Jonathan Mejorado Lopez

  License:
  BSD-3-Clause (see the LICENSE file at the root of the library).
  Based on the Sensirion arduino-i2c-scd4x library.
  Copyright (c) 2025, Sensirion AG
  Copyright (c) 2026, UNIT Electronics
*/

#ifndef DEVLAB_SCD4X_H
#define DEVLAB_SCD4X_H

#include <Arduino.h>
#include <Wire.h>
#include <DevLab_I2C_Interface.h>

class DevLab_SCD4x {
public:
    /* Fixed I2C address of every SCD4x. */
    static constexpr uint8_t DEFAULT_ADDRESS = 0x62U;

    struct Measurement {
        uint16_t co2 = 0;            // CO2 concentration [ppm]
        float temperatureC = 0.0f;   // temperature [degC]
        float humidity = 0.0f;       // relative humidity [%RH]
    };

    enum class Variant : uint8_t { Unknown = 0, SCD40, SCD41, SCD42, SCD43 };

    enum class Error : uint8_t {
        None = 0,
        Write,          // sensor did not ACK the command (not connected / asleep / command not allowed now)
        Read,           // sensor did not answer the read (e.g. no data ready yet)
        Crc,            // a received data word failed its CRC-8
        Timeout,        // data did not become ready in time
        Argument,       // value out of range
        NotConnected,   // begin() could not talk to the sensor
        Failed          // the sensor executed the command but reported failure (FRC)
    };

    /**
     * @param wire    I2C bus to use.
     * @param address I2C address (0x62 on every SCD4x).
     * @param clock   I2C clock in Hz. The SCD4x supports up to 100 kHz.
     */
    explicit DevLab_SCD4x(TwoWire &wire = Wire, uint8_t address = DEFAULT_ADDRESS,
                          uint32_t clock = 100000UL);

    /**
     * Initializes the bus with the default pins of `wire`, wakes up and
     * verifies the sensor, and (by default) starts the periodic measurement
     * (one sample every 5 s).
     * @param startMeasurement false to leave the sensor idle (needed before
     *        changing settings or using single-shot mode).
     * @return true if the sensor answered and is ready.
     */
    bool begin(bool startMeasurement = true);

    /** Same as begin(), with explicit SDA/SCL pins and I2C clock. */
    bool begin(uint8_t sdaPin, uint8_t sclPin, uint32_t clock = 100000UL,
               bool startMeasurement = true);

    /* ---------------- Measurement ---------------- */

    /** Starts the periodic measurement: a new sample every 5 s. */
    bool startPeriodic();
    /** Low-power periodic measurement: a new sample every 30 s. */
    bool startLowPowerPeriodic();
    /** Stops the periodic measurement (takes 500 ms). */
    bool stopPeriodic();

    /** true when a new sample is waiting to be read. */
    bool dataReady(bool &ready);
    /** Same as dataReady(), returns false on a bus error too. */
    bool available();

    /** Waits for a new sample (up to timeoutMs) and reads it. */
    bool read(Measurement &m, uint32_t timeoutMs = 6000UL);

    /** Reads the sample right now; fails with Error::Read if none is ready. */
    bool readMeasurement(Measurement &m);

    /** SCD41/SCD43 only. Idle mode: one measurement on demand, blocks ~5 s. */
    bool measureSingleShot(Measurement &m);
    /** SCD41/SCD43 only. Temperature and humidity only, ~50 ms. CO2 is not
     *  measured in this mode: m.co2 is always set to 0. */
    bool measureSingleShotRhtOnly(Measurement &m);

    /* ---------------- Settings (sensor must be idle unless noted) ---------------- */

    bool setTemperatureOffset(float offsetC);        // [degC], 0..175 representable
    bool getTemperatureOffset(float &offsetC);
    bool setAltitude(uint16_t meters);
    bool getAltitude(uint16_t &meters);
    /** Ambient pressure in Pa. Allowed while measuring. */
    bool setAmbientPressure(uint32_t pascal);
    bool getAmbientPressure(uint32_t &pascal);

    bool setAutoCalibration(bool enabled);           // ASC
    bool getAutoCalibration(bool &enabled);
    bool setAutoCalibrationTarget(uint16_t ppm);
    bool getAutoCalibrationTarget(uint16_t &ppm);
    bool setAutoCalibrationInitialPeriod(uint16_t hours);
    bool getAutoCalibrationInitialPeriod(uint16_t &hours);
    bool setAutoCalibrationStandardPeriod(uint16_t hours);
    bool getAutoCalibrationStandardPeriod(uint16_t &hours);

    /** Stores the current settings in the sensor EEPROM (800 ms). Limited write cycles. */
    bool persistSettings();

    /* ---------------- Calibration / maintenance (sensor must be idle) ---------------- */

    /** Forced recalibration against a known CO2 concentration; returns the applied correction in ppm. */
    bool performForcedRecalibration(uint16_t targetPpm, int16_t &correctionPpm);
    /** Self test (blocks 10 s). ok = true when no malfunction was found. */
    bool performSelfTest(bool &ok);
    /** Resets every setting to factory defaults (1.2 s). */
    bool performFactoryReset();
    /** Reloads the settings from EEPROM (30 ms). */
    bool reinit();

    /* ---------------- Power (SCD41/SCD43) ---------------- */

    bool powerDown();
    bool wakeUp();

    /* ---------------- Identification ---------------- */

    /** 48-bit serial number: the three received words joined (word0:word1:word2). The CRC is checked. */
    bool getSerialNumber(uint64_t &serial);
    /**
     * Serial number response exactly as the sensor sends it: 9 bytes = 3 words,
     * each one [msb, lsb, crc]. The CRC is NOT checked, so the frame is returned
     * even if a CRC does not match; crcOk (optional) reports whether the three
     * CRCs match. Fails only if the I2C transfer fails.
     */
    bool getSerialNumberRaw(uint8_t raw[9], bool *crcOk = nullptr);
    /** Prints the raw serial number frame with the received and the calculated CRC of each word. */
    void printSerialNumberRaw(Print &out = Serial);
    bool getVariant(Variant &variant);

    bool busReady() const { return _busReady; }
    bool isConnected() const { return _connected; }
    uint8_t address() const { return _address; }
    Variant variant() const { return _variant; }
    Error lastError() const { return _lastError; }
    static const char *errorString(Error e);
    void printInfo(Print &out = Serial);

private:
    // Command ids.
    enum : uint16_t {
        CMD_START_PERIODIC = 0x21B1,
        CMD_READ_MEASUREMENT = 0xEC05,
        CMD_STOP_PERIODIC = 0x3F86,
        CMD_SET_TEMPERATURE_OFFSET = 0x241D,
        CMD_GET_TEMPERATURE_OFFSET = 0x2318,
        CMD_SET_ALTITUDE = 0x2427,
        CMD_GET_ALTITUDE = 0x2322,
        CMD_SET_AMBIENT_PRESSURE = 0xE000,
        CMD_FORCED_RECALIBRATION = 0x362F,
        CMD_SET_ASC_ENABLED = 0x2416,
        CMD_GET_ASC_ENABLED = 0x2313,
        CMD_SET_ASC_TARGET = 0x243A,
        CMD_GET_ASC_TARGET = 0x233F,
        CMD_START_LOW_POWER_PERIODIC = 0x21AC,
        CMD_GET_DATA_READY = 0xE4B8,
        CMD_PERSIST_SETTINGS = 0x3615,
        CMD_GET_SERIAL_NUMBER = 0x3682,
        CMD_SELF_TEST = 0x3639,
        CMD_FACTORY_RESET = 0x3632,
        CMD_REINIT = 0x3646,
        CMD_GET_VARIANT = 0x202F,
        CMD_SINGLE_SHOT = 0x219D,
        CMD_SINGLE_SHOT_RHT_ONLY = 0x2196,
        CMD_POWER_DOWN = 0x36E0,
        CMD_WAKE_UP = 0x36F6,
        CMD_SET_ASC_INITIAL_PERIOD = 0x2445,
        CMD_GET_ASC_INITIAL_PERIOD = 0x2340,
        CMD_SET_ASC_STANDARD_PERIOD = 0x244E,
        CMD_GET_ASC_STANDARD_PERIOD = 0x234B
    };

    static constexpr uint8_t kMaxArgs = 1;    // data words sent with a command
    static constexpr uint8_t kMaxWords = 3;   // data words received from a command

    bool beginCommon(bool startMeasurement);
    // rawOut (optional) receives the response bytes as read from the bus; with
    // checkCrc == false the CRC of the received words is not verified.
    bool transfer(uint16_t cmd, const uint16_t *args, uint8_t nArgs, uint16_t execMs,
                  uint16_t *resp, uint8_t nResp, uint8_t *rawOut = nullptr, bool checkCrc = true);
    bool command(uint16_t cmd, uint16_t execMs = 0) { return transfer(cmd, nullptr, 0, execMs, nullptr, 0); }
    bool writeWord(uint16_t cmd, uint16_t value, uint16_t execMs = 1) { return transfer(cmd, &value, 1, execMs, nullptr, 0); }
    bool readWord(uint16_t cmd, uint16_t &value, uint16_t execMs = 1) { return transfer(cmd, nullptr, 0, execMs, &value, 1); }
    bool fail(Error e) { _lastError = e; return false; }

    static uint8_t crc8(const uint8_t *data, size_t len);

    DevLab_I2C_Interface _bus;
    uint8_t _address;
    bool _busReady = false;
    bool _connected = false;
    Variant _variant = Variant::Unknown;
    Error _lastError = Error::None;
};

#endif // DEVLAB_SCD4X_H
