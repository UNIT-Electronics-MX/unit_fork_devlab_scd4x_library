# DevLab SCD4x Arduino Library

Arduino library for the Sensirion SCD4x CO₂, temperature and relative humidity
sensors over I²C. It is part of the DevLab ecosystem: all the bus handling is
done inside the `DevLab_SCD4x` class on top of `DevLab_Interface`, so a sketch
only creates the object, calls `begin()` and reads measurements.

<img src="images/SCD4x.png" width="300px">

Click [here](https://sensirion.com/products/catalog/SEK-SCD43) to learn more about the Sensirion SCD4x sensor family.


## Supported sensor types

| Sensor name   | I²C Addresses  | Notes |
| ------------- | -------------- | ----- |
|[SCD40](https://sensirion.com/products/catalog/SCD40)| **0x62**| |
|[SCD41](https://sensirion.com/products/catalog/SCD41)| **0x62**| Single-shot measurement and power down. Tested sensor |
|[SCD42](https://sensirion.com/products/catalog/SCD42)| **0x62**| |
|[SCD43](https://sensirion.com/products/catalog/SCD43)| **0x62**| Single-shot measurement and power down |

Single-shot measurement and power down are only available on the SCD41 and SCD43.
The examples use an *SCD41*.

## Supported platforms

The examples select the I²C pins per architecture: RP2040/RP2350 (arduino-pico),
ESP32, STM32 (STM32duino) and AVR.


## Installation of the library

Download the latest release as a .zip file and add it to the
[Arduino IDE](http://www.arduino.cc/en/main/software) via

`Sketch` ➔ `Include Library` ➔ `Add .ZIP Library...`

Install the dependency listed below the same way, or via the Library Manager.

#### Dependencies
* DevLab_Interface (>=1.3.0): provides the I²C transport, including the raw
  `writeRaw()` / `readRaw()` transfers used by the SCD4x command protocol.

## Connect the sensor

Use the following pin description to connect your SCD4X to the standard I²C bus of your Arduino board:

<img src="images/SCD4x_pinout.png" width="300px">

| *Pin* | *Cable Color* | *Name* | *Description*  | *Comments* |
|-------|---------------|:------:|----------------|------------|
| 1 | black | GND | Ground |
| 2 | red | VDD | Supply Voltage | 2.4V to 5.5V
| 3 | green or blue | SDA | I2C: Serial data input / output |
| 4 | yellow | SCL | I2C: Serial clock input |

The recommended voltage is 3.3V.

### Board specific wiring
You will find pinout schematics for recommended board models below:



<details><summary>Arduino Uno</summary>
<p>

| *SCD4X* | *SCD4X Pin* | *Cable Color* | *Board Pin* |
| :---: | --- | --- | --- |
| GND | 1 | black | GND |
| VDD | 2 | red | 3.3V |
| SDA | 3 | green or blue | D18/SDA |
| SCL | 4 | yellow | D19/SCL |



<img src="images/Arduino-Uno-Rev3-i2c-pinout-3.3V.png" width="600px">
</p>
</details>




<details><summary>Arduino Nano</summary>
<p>

| *SCD4X* | *SCD4X Pin* | *Cable Color* | *Board Pin* |
| :---: | --- | --- | --- |
| GND | 1 | black | GND |
| VDD | 2 | red | 3.3V |
| SDA | 3 | green or blue | A4 |
| SCL | 4 | yellow | A5 |



<img src="images/Arduino-Nano-i2c-pinout-3.3V.png" width="600px">
</p>
</details>




<details><summary>Arduino Micro</summary>
<p>

| *SCD4X* | *SCD4X Pin* | *Cable Color* | *Board Pin* |
| :---: | --- | --- | --- |
| GND | 1 | black | GND |
| VDD | 2 | red | 3.3V |
| SDA | 3 | green or blue | D2/SDA |
| SCL | 4 | yellow | ~D3/SCL |



<img src="images/Arduino-Micro-i2c-pinout-3.3V.png" width="600px">
</p>
</details>




<details><summary>Arduino Mega 2560</summary>
<p>

| *SCD4X* | *SCD4X Pin* | *Cable Color* | *Board Pin* |
| :---: | --- | --- | --- |
| GND | 1 | black | GND |
| VDD | 2 | red | 3.3V |
| SDA | 3 | green or blue | D20/SDA |
| SCL | 4 | yellow | D21/SCL |



<img src="images/Arduino-Mega-2560-Rev3-i2c-pinout-3.3V.png" width="600px">
</p>
</details>




<details><summary>ESP32 DevKitC</summary>
<p>

| *SCD4X* | *SCD4X Pin* | *Cable Color* | *Board Pin* |
| :---: | --- | --- | --- |
| GND | 1 | black | GND |
| VDD | 2 | red | 3V3 |
| SDA | 3 | green or blue | GPIO 21 |
| SCL | 4 | yellow | GPIO 22 |



<img src="images/esp32-devkitc-i2c-pinout-3.3V.png" width="600px">
</p>
</details>



## Quick Start

1. Install the library and its dependency according to [Installation of the library](#installation-of-the-library)

2. Connect the SCD4x sensor to your board as explained in [Connect the sensor](#connect-the-sensor)

3. Open the `exampleUsage` sample project within the Arduino IDE:

   `File` ➔ `Examples` ➔ `DevLab_SCD4x` ➔ `exampleUsage`

   The example selects the I²C pins according to the board architecture. Change
   `I2C_SDA` / `I2C_SCL` at the top of the sketch if your board is wired differently.

4. Click the `Upload` button in the Arduino IDE or `Sketch` ➔ `Upload`

5. When the upload process has finished, open the `Serial Monitor` or `Serial
   Plotter` via the `Tools` menu to observe the measurement values. Note that
   the `Baud Rate` in the used tool has to be set to `115200 baud`.

## Examples

| Example | Description |
|---|---|
| `exampleUsage` | Periodic measurement: prints CO₂, temperature and humidity every 5 s |
| `exampleScd41SingleShot` | On-demand measurement with power down between samples (SCD41/SCD43) |
| `exampleSerialNumberRaw` | Reads the serial number frame as sent by the sensor (9 bytes) and checks the CRC of each word |

## Usage

```cpp
#include <DevLab_SCD4x.h>

DevLab_SCD4x scd(Wire);                  // I2C address 0x62 by default

void setup() {
  Serial.begin(115200);
  scd.begin(SDA_PIN, SCL_PIN);           // starts the periodic measurement (5 s)
}

void loop() {
  DevLab_SCD4x::Measurement m;
  if (scd.read(m))                       // waits for a new sample
    Serial.printf("CO2=%u ppm  T=%.2f C  RH=%.2f %%\n", m.co2, m.temperatureC, m.humidity);
}
```

`begin()` waits for the sensor power-up, brings it to idle (waking it up if it was
powered down), verifies the serial number and starts the periodic measurement.
Use `begin(sda, scl, clock, false)` to leave the sensor idle, which is required to
change settings or to use single-shot mode. The I²C clock must not exceed 100 kHz.

## API

### Measurement

| Method | Description |
|---|---|
| `startPeriodic()` / `stopPeriodic()` | Start the periodic measurement (new sample every 5 s) / stop it (500 ms) |
| `startLowPowerPeriodic()` | Periodic measurement with a new sample every 30 s |
| `dataReady(ready)` / `available()` | Whether a new sample is waiting |
| `read(m, timeoutMs)` | Waits for a new sample and reads it |
| `readMeasurement(m)` | Reads the sample immediately; fails if none is ready |
| `measureSingleShot(m)` | SCD41/SCD43. On-demand measurement, blocks about 5 s |
| `measureSingleShotRhtOnly(m)` | SCD41/SCD43. Temperature and humidity only (`m.co2` is 0), about 50 ms |

`Measurement` holds `co2` [ppm], `temperatureC` [°C] and `humidity` [%RH].

### Settings

Settings are changed with the sensor idle, except `setAmbientPressure()`, which is also allowed while measuring.

| Method | Description |
|---|---|
| `setTemperatureOffset(°C)` / `getTemperatureOffset()` | Temperature offset |
| `setAltitude(m)` / `getAltitude()` | Sensor altitude |
| `setAmbientPressure(Pa)` / `getAmbientPressure()` | Ambient pressure compensation |
| `setAutoCalibration(on)` / `getAutoCalibration()` | Automatic self-calibration (ASC) |
| `setAutoCalibrationTarget()` / `get…` | ASC target CO₂ concentration [ppm] |
| `setAutoCalibrationInitialPeriod()` / `get…` | ASC initial period [h] |
| `setAutoCalibrationStandardPeriod()` / `get…` | ASC standard period [h] |
| `persistSettings()` | Stores the settings in the sensor EEPROM (800 ms, limited write cycles) |

### Calibration and maintenance (sensor idle)

| Method | Description |
|---|---|
| `performForcedRecalibration(ppm, correction)` | Forced recalibration against a reference concentration (400 ms) |
| `performSelfTest(ok)` | Sensor self test (blocks 10 s) |
| `performFactoryReset()` | Restores the factory settings (1.2 s) |
| `reinit()` | Reloads the settings from the EEPROM (30 ms) |

### Power (SCD41/SCD43)

| Method | Description |
|---|---|
| `powerDown()` | Puts the sensor to sleep |
| `wakeUp()` | Wakes it up (30 ms) |

### Information and errors

| Method | Description |
|---|---|
| `getSerialNumber(serial)` | 48-bit serial number: the three received words joined as `word0:word1:word2`. The CRC is checked |
| `getSerialNumberRaw(raw, crcOk)` | The serial number response exactly as received: 9 bytes, three words each `[msb, lsb, crc]`. The CRC is not checked, `crcOk` reports it |
| `printSerialNumberRaw(Print&)` | Prints the raw frame with the received and the calculated CRC-8 of each word |
| `getVariant(variant)` | Sensor variant (SCD40/41/42/43) |
| `isConnected()` / `busReady()` / `address()` / `variant()` | State of the object |
| `printInfo(Print&)` | Prints the variant and the serial number |
| `lastError()` / `errorString(e)` | Error of the last failed call |

Every method returns `true` on success and `false` on failure. `lastError()` returns one of
`Error::Write` (no ACK), `Error::Read` (no response or no data ready), `Error::Crc`,
`Error::Timeout`, `Error::Argument`, `Error::NotConnected` and `Error::Failed`.

While the periodic measurement is running, only `readMeasurement()`, `dataReady()`,
`stopPeriodic()` and `setAmbientPressure()` are meant to be used. Other commands are
not supported in that mode and may fail or behave unpredictably: call `stopPeriodic()` first.

## Contributing

**Contributions are welcome!**

This library uses
[`clang-format`](https://releases.llvm.org/download.html) to standardize the
formatting of all our `.cpp` and `.h` files. Make sure your contributions are
formatted accordingly:

The `-i` flag will apply the format changes to the files listed.

```bash
clang-format -i src/*.cpp src/*.h
```

Note that differences from this formatting will result in a failed build until
they are fixed.
:

## License

See [LICENSE](LICENSE).
