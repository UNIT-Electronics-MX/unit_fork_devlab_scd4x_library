/*
  DevLab_SCD4x.cpp

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

#include "DevLab_SCD4x.h"

DevLab_SCD4x::DevLab_SCD4x(TwoWire &wire, uint8_t address, uint32_t clock)
    : _bus(wire, address, clock), _address(address) {
}

/* ------------------------------------------------------------------ */
/* Protocol layer                                                      */
/* ------------------------------------------------------------------ */

uint8_t DevLab_SCD4x::crc8(const uint8_t *data, size_t len) {
    uint8_t crc = 0xFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; bit++) {
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
        }
    }
    return crc;
}

/*
 * One complete SCD4x transaction:
 *   write  [cmd_hi cmd_lo] ([arg_hi arg_lo crc])*  + STOP
 *   wait   execMs
 *   read   ([word_hi word_lo crc])*nResp            (only if nResp > 0)
 * Both phases use DevLab_I2C_Interface::writeRaw()/readRaw(): the command is
 * sent as-is with a STOP, and the response is a plain read with its own START.
 */
bool DevLab_SCD4x::transfer(uint16_t cmd, const uint16_t *args, uint8_t nArgs,
                            uint16_t execMs, uint16_t *resp, uint8_t nResp,
                            uint8_t *rawOut, bool checkCrc) {
    if (nArgs > kMaxArgs || nResp > kMaxWords) {
        return fail(Error::Argument);
    }

    uint8_t tx[2 + 3 * kMaxArgs];
    size_t txLen = 0;
    tx[txLen++] = (uint8_t)(cmd >> 8);
    tx[txLen++] = (uint8_t)(cmd & 0xFF);
    for (uint8_t i = 0; i < nArgs; i++) {
        tx[txLen++] = (uint8_t)(args[i] >> 8);
        tx[txLen++] = (uint8_t)(args[i] & 0xFF);
        tx[txLen] = crc8(&tx[txLen - 2], 2);
        txLen++;
    }
    if (!_bus.writeRaw(tx, txLen)) {
        return fail(Error::Write);
    }

    if (execMs > 0) {
        delay(execMs);
    }

    if (nResp > 0) {
        uint8_t rx[3 * kMaxWords];
        const size_t rxLen = 3U * nResp;
        if (!_bus.readRaw(rx, rxLen)) {
            return fail(Error::Read);
        }
        if (rawOut != nullptr) {
            memcpy(rawOut, rx, rxLen);
        }
        for (uint8_t i = 0; i < nResp; i++) {
            const uint8_t *w = &rx[3 * i];
            if (checkCrc && crc8(w, 2) != w[2]) {
                return fail(Error::Crc);
            }
            if (resp != nullptr) {
                resp[i] = (uint16_t)(((uint16_t)w[0] << 8) | w[1]);
            }
        }
    }

    _lastError = Error::None;
    return true;
}

/* ------------------------------------------------------------------ */
/* Lifecycle                                                           */
/* ------------------------------------------------------------------ */

bool DevLab_SCD4x::begin(bool startMeasurement) {
    _busReady = _bus.begin();
    return beginCommon(startMeasurement);
}

bool DevLab_SCD4x::begin(uint8_t sdaPin, uint8_t sclPin, uint32_t clock, bool startMeasurement) {
    _busReady = _bus.begin(sdaPin, sclPin);
    _bus.setClock(clock);
    return beginCommon(startMeasurement);
}

bool DevLab_SCD4x::beginCommon(bool startMeasurement) {
    _connected = false;
    _variant = Variant::Unknown;
    if (!_busReady) {
        return fail(Error::NotConnected);
    }

    // The sensor needs up to 1 s after power-up before it accepts commands.
    if (millis() < 1000UL) {
        delay(1000UL - millis());
    }

    // Bring it to idle: stop any running measurement. After an MCU reset the
    // sensor can need a few seconds to answer, and a SCD41 may be powered down.
    bool idle = false;
    for (uint8_t attempt = 0; attempt < 10 && !idle; attempt++) {
        idle = stopPeriodic();
        if (!idle) {
            if (attempt == 1) {
                wakeUp();       // not acknowledged by the sensor; harmless if awake
            }
            delay(300);
        }
    }
    if (!idle) {
        return fail(Error::NotConnected);
    }

    // Verify that it really is an SCD4x: a valid serial number (CRC checked).
    uint64_t serial = 0;
    if (!getSerialNumber(serial)) {
        return fail(Error::NotConnected);
    }
    _connected = true;

    Variant v;
    if (getVariant(v)) {
        _variant = v;
    }

    if (startMeasurement && !startPeriodic()) {
        return false;
    }
    _lastError = Error::None;
    return true;
}

/* ------------------------------------------------------------------ */
/* Measurement                                                         */
/* ------------------------------------------------------------------ */

bool DevLab_SCD4x::startPeriodic() { return command(CMD_START_PERIODIC); }
bool DevLab_SCD4x::startLowPowerPeriodic() { return command(CMD_START_LOW_POWER_PERIODIC); }
bool DevLab_SCD4x::stopPeriodic() { return command(CMD_STOP_PERIODIC, 500); }

bool DevLab_SCD4x::dataReady(bool &ready) {
    uint16_t status = 0;
    if (!readWord(CMD_GET_DATA_READY, status)) {
        return false;
    }
    ready = (status & 0x07FF) != 0;
    return true;
}

bool DevLab_SCD4x::available() {
    bool ready = false;
    return dataReady(ready) && ready;
}

bool DevLab_SCD4x::readMeasurement(Measurement &m) {
    uint16_t w[3];
    if (!transfer(CMD_READ_MEASUREMENT, nullptr, 0, 1, w, 3)) {
        return false;
    }
    m.co2 = w[0];
    m.temperatureC = -45.0f + (175.0f * w[1]) / 65535.0f;
    m.humidity = (100.0f * w[2]) / 65535.0f;
    return true;
}

bool DevLab_SCD4x::read(Measurement &m, uint32_t timeoutMs) {
    const uint32_t start = millis();
    for (;;) {
        bool ready = false;
        if (!dataReady(ready)) {
            return false;
        }
        if (ready) {
            break;
        }
        if ((uint32_t)(millis() - start) >= timeoutMs) {
            return fail(Error::Timeout);
        }
        delay(100);
    }
    return readMeasurement(m);
}

bool DevLab_SCD4x::measureSingleShot(Measurement &m) {
    if (!command(CMD_SINGLE_SHOT, 5000)) {
        return false;
    }
    return read(m, 2000);
}

bool DevLab_SCD4x::measureSingleShotRhtOnly(Measurement &m) {
    if (!command(CMD_SINGLE_SHOT_RHT_ONLY, 50)) {
        return false;
    }
    if (!read(m, 2000)) {
        return false;
    }
    // This mode does not measure CO2. The sensor still returns a word for it,
    // but it has no meaning (a previous value or an arbitrary one): report 0.
    m.co2 = 0;
    return true;
}

/* ------------------------------------------------------------------ */
/* Settings                                                            */
/* ------------------------------------------------------------------ */

bool DevLab_SCD4x::setTemperatureOffset(float offsetC) {
    if (offsetC < 0.0f || offsetC > 175.0f) {
        return fail(Error::Argument);
    }
    return writeWord(CMD_SET_TEMPERATURE_OFFSET, (uint16_t)((offsetC * 65535.0f) / 175.0f + 0.5f));
}

bool DevLab_SCD4x::getTemperatureOffset(float &offsetC) {
    uint16_t raw = 0;
    if (!readWord(CMD_GET_TEMPERATURE_OFFSET, raw)) {
        return false;
    }
    offsetC = (175.0f * raw) / 65535.0f;
    return true;
}

bool DevLab_SCD4x::setAltitude(uint16_t meters) { return writeWord(CMD_SET_ALTITUDE, meters); }
bool DevLab_SCD4x::getAltitude(uint16_t &meters) { return readWord(CMD_GET_ALTITUDE, meters); }

bool DevLab_SCD4x::setAmbientPressure(uint32_t pascal) {
    const uint32_t hPa = (pascal + 50UL) / 100UL;
    if (hPa > 0xFFFFUL) {
        return fail(Error::Argument);
    }
    return writeWord(CMD_SET_AMBIENT_PRESSURE, (uint16_t)hPa);
}

bool DevLab_SCD4x::getAmbientPressure(uint32_t &pascal) {
    uint16_t hPa = 0;
    // set and get share the command id; get sends no argument.
    if (!readWord(CMD_SET_AMBIENT_PRESSURE, hPa)) {
        return false;
    }
    pascal = (uint32_t)hPa * 100UL;
    return true;
}

bool DevLab_SCD4x::setAutoCalibration(bool enabled) { return writeWord(CMD_SET_ASC_ENABLED, enabled ? 1 : 0); }

bool DevLab_SCD4x::getAutoCalibration(bool &enabled) {
    uint16_t v = 0;
    if (!readWord(CMD_GET_ASC_ENABLED, v)) {
        return false;
    }
    enabled = (v != 0);
    return true;
}

bool DevLab_SCD4x::setAutoCalibrationTarget(uint16_t ppm) { return writeWord(CMD_SET_ASC_TARGET, ppm); }
bool DevLab_SCD4x::getAutoCalibrationTarget(uint16_t &ppm) { return readWord(CMD_GET_ASC_TARGET, ppm); }
bool DevLab_SCD4x::setAutoCalibrationInitialPeriod(uint16_t hours) { return writeWord(CMD_SET_ASC_INITIAL_PERIOD, hours); }
bool DevLab_SCD4x::getAutoCalibrationInitialPeriod(uint16_t &hours) { return readWord(CMD_GET_ASC_INITIAL_PERIOD, hours); }
bool DevLab_SCD4x::setAutoCalibrationStandardPeriod(uint16_t hours) { return writeWord(CMD_SET_ASC_STANDARD_PERIOD, hours); }
bool DevLab_SCD4x::getAutoCalibrationStandardPeriod(uint16_t &hours) { return readWord(CMD_GET_ASC_STANDARD_PERIOD, hours); }

bool DevLab_SCD4x::persistSettings() { return command(CMD_PERSIST_SETTINGS, 800); }

/* ------------------------------------------------------------------ */
/* Calibration / maintenance                                           */
/* ------------------------------------------------------------------ */

bool DevLab_SCD4x::performForcedRecalibration(uint16_t targetPpm, int16_t &correctionPpm) {
    uint16_t raw = 0;
    if (!transfer(CMD_FORCED_RECALIBRATION, &targetPpm, 1, 400, &raw, 1)) {
        return false;
    }
    if (raw == 0xFFFF) {
        return fail(Error::Failed);       // FRC failed (sensor was not running long enough)
    }
    correctionPpm = (int16_t)((int32_t)raw - 0x8000);
    return true;
}

bool DevLab_SCD4x::performSelfTest(bool &ok) {
    uint16_t status = 0;
    if (!readWord(CMD_SELF_TEST, status, 10000)) {
        return false;
    }
    ok = (status == 0);
    return true;
}

bool DevLab_SCD4x::performFactoryReset() { return command(CMD_FACTORY_RESET, 1200); }
bool DevLab_SCD4x::reinit() { return command(CMD_REINIT, 30); }

/* ------------------------------------------------------------------ */
/* Power                                                               */
/* ------------------------------------------------------------------ */

bool DevLab_SCD4x::powerDown() { return command(CMD_POWER_DOWN, 1); }

bool DevLab_SCD4x::wakeUp() {
    // The sensor does not acknowledge wake_up, so a NACK here is expected.
    const uint8_t cmd[2] = {(uint8_t)(CMD_WAKE_UP >> 8), (uint8_t)(CMD_WAKE_UP & 0xFF)};
    _bus.writeRaw(cmd, sizeof cmd);
    delay(30);
    _lastError = Error::None;
    return true;
}

/* ------------------------------------------------------------------ */
/* Identification                                                      */
/* ------------------------------------------------------------------ */

bool DevLab_SCD4x::getSerialNumber(uint64_t &serial) {
    uint16_t w[3];
    if (!transfer(CMD_GET_SERIAL_NUMBER, nullptr, 0, 1, w, 3)) {
        return false;
    }
    serial = ((uint64_t)w[0] << 32) | ((uint64_t)w[1] << 16) | (uint64_t)w[2];
    return true;
}

bool DevLab_SCD4x::getSerialNumberRaw(uint8_t raw[9], bool *crcOk) {
    if (!transfer(CMD_GET_SERIAL_NUMBER, nullptr, 0, 1, nullptr, 3, raw, false)) {
        return false;
    }
    if (crcOk != nullptr) {
        *crcOk = true;
        for (uint8_t i = 0; i < 3; i++) {
            if (crc8(&raw[3 * i], 2) != raw[3 * i + 2]) {
                *crcOk = false;
            }
        }
    }
    return true;
}

void DevLab_SCD4x::printSerialNumberRaw(Print &out) {
    uint8_t raw[9];
    if (!getSerialNumberRaw(raw)) {
        out.print("serial number raw: read failed (");
        out.print(errorString(_lastError));
        out.println(")");
        return;
    }
    out.print("serial number raw (9 bytes):");
    for (uint8_t i = 0; i < 9; i++) {
        out.print(i % 3 == 0 ? "  " : " ");
        if (raw[i] < 0x10) {
            out.print('0');
        }
        out.print(raw[i], HEX);
    }
    out.println();
    for (uint8_t w = 0; w < 3; w++) {
        const uint8_t *b = &raw[3 * w];
        const uint8_t calc = crc8(b, 2);
        out.print("  word ");
        out.print(w);
        out.print(": data 0x");
        if (b[0] < 0x10) out.print('0');
        out.print(b[0], HEX);
        if (b[1] < 0x10) out.print('0');
        out.print(b[1], HEX);
        out.print("  crc received 0x");
        if (b[2] < 0x10) out.print('0');
        out.print(b[2], HEX);
        out.print("  crc calculated 0x");
        if (calc < 0x10) out.print('0');
        out.print(calc, HEX);
        out.println(calc == b[2] ? "  OK" : "  MISMATCH");
    }
}

bool DevLab_SCD4x::getVariant(Variant &variant) {
    uint16_t raw = 0;
    if (!readWord(CMD_GET_VARIANT, raw)) {
        return false;
    }
    switch ((raw >> 12) & 0x0F) {
        case 0x0: variant = Variant::SCD40; break;
        case 0x1: variant = Variant::SCD41; break;
        case 0x2: variant = Variant::SCD42; break;
        case 0x5: variant = Variant::SCD43; break;
        default:  variant = Variant::Unknown; break;
    }
    return true;
}

const char *DevLab_SCD4x::errorString(Error e) {
    switch (e) {
        case Error::None:         return "no error";
        case Error::Write:        return "sensor did not ACK the command";
        case Error::Read:         return "no response / no data ready";
        case Error::Crc:          return "CRC error";
        case Error::Timeout:      return "timeout";
        case Error::Argument:     return "argument out of range";
        case Error::NotConnected: return "sensor not found";
        case Error::Failed:       return "command failed in the sensor";
    }
    return "unknown";
}

void DevLab_SCD4x::printInfo(Print &out) {
    static const char *const names[] = {"unknown", "SCD40", "SCD41", "SCD42", "SCD43"};
    uint64_t serial = 0;
    out.print("SCD4x variant: ");
    out.println(names[(uint8_t)_variant]);
    if (getSerialNumber(serial)) {
        out.print("serial number: 0x");
        for (int shift = 44; shift >= 0; shift -= 4) {
            out.print((uint8_t)((serial >> shift) & 0x0F), HEX);
        }
        out.println();
    }
}