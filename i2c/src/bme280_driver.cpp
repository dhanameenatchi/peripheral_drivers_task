#include <zephyr/logging/log.h>

#ifdef ZEPHYR_BUILD
LOG_MODULE_REGISTER(bme280_driver, LOG_LEVEL_INF);
#else
#include <zephyr/logging/log.h>
#endif
#include "bme280_driver.hpp"
#include <cmath>

BME280Driver::BME280Driver(uint8_t addr) : bus_(addr)
{
    uint8_t id = 0;
    if (!bus_.readRegs(REG_CHIP_ID, &id, 1))
    {
        LOG_ERR("Failed to read BME280 Chip ID");
        return;
    }
    if (id != CHIP_ID)
    {
        LOG_ERR("Unexpected BME280 Chip ID: 0x%02X", id);
        return;
    }
    LOG_INF("BME280 detected");

    if (!readCalibrationData())
    {
        LOG_ERR("Failed to read BME280 calibration data");
        return;
    }
    calib_valid_ = true;

    // ctrl_hum MUST be written before ctrl_meas for humidity oversampling
    // to take effect (datasheet §5.4.3: "Changes to this register only
    // become effective after a write operation to ctrl_meas").
    // osrs_h = 001 -> humidity oversampling x1
    if (!bus_.writeReg(REG_CTRL_HUM, 0x01))
    {
        LOG_ERR("Failed to configure BME280 ctrl_hum");
    }

    // osrs_t=010 (x2), osrs_p=101 (x16), mode=11 (normal) -> 0x57
    if (!bus_.writeReg(REG_CTRL_MEAS, 0x57))
    {
        LOG_ERR("Failed to configure BME280 ctrl_meas");
    }
}

bool BME280Driver::readCalibrationData()
{
    // --- T1..P9 block: 0x88..0x9F, 24 bytes, little-endian 16-bit words ---
    uint8_t buf1[24] = {};
    if (!bus_.readRegs(REG_CALIB00, buf1, sizeof(buf1)))
    {
        return false;
    }

    auto u16 = [](uint8_t lo, uint8_t hi) -> uint16_t {
        return static_cast<uint16_t>(lo) | (static_cast<uint16_t>(hi) << 8);
    };
    auto s16 = [&](uint8_t lo, uint8_t hi) -> int16_t {
        return static_cast<int16_t>(u16(lo, hi));
    };

    calib_.dig_T1 = u16(buf1[0],  buf1[1]);
    calib_.dig_T2 = s16(buf1[2],  buf1[3]);
    calib_.dig_T3 = s16(buf1[4],  buf1[5]);

    calib_.dig_P1 = u16(buf1[6],  buf1[7]);
    calib_.dig_P2 = s16(buf1[8],  buf1[9]);
    calib_.dig_P3 = s16(buf1[10], buf1[11]);
    calib_.dig_P4 = s16(buf1[12], buf1[13]);
    calib_.dig_P5 = s16(buf1[14], buf1[15]);
    calib_.dig_P6 = s16(buf1[16], buf1[17]);
    calib_.dig_P7 = s16(buf1[18], buf1[19]);
    calib_.dig_P8 = s16(buf1[20], buf1[21]);
    calib_.dig_P9 = s16(buf1[22], buf1[23]);

    // --- dig_H1: single byte at 0xA1 ---
    uint8_t h1 = 0;
    if (!bus_.readRegs(REG_CALIB_H1, &h1, 1))
    {
        return false;
    }
    calib_.dig_H1 = h1;

    // --- H2..H6 block: 0xE1..0xE7, 7 bytes ---
    uint8_t buf2[7] = {};
    if (!bus_.readRegs(REG_CALIB26, buf2, sizeof(buf2)))
    {
        return false;
    }

    // dig_H2: 0xE1/0xE2, signed short, little-endian
    calib_.dig_H2 = s16(buf2[0], buf2[1]);

    // dig_H3: 0xE3, unsigned char
    calib_.dig_H3 = buf2[2];

    // dig_H4: 0xE4[7:0] holds bits [11:4], 0xE5[3:0] holds bits [3:0]
    // dig_H5: 0xE5[7:4] holds bits [3:0], 0xE6[7:0] holds bits [11:4]
    // Per datasheet Table 16:
    //   0xE4 / 0xE5[3:0]  -> dig_H4 [11:4] / [3:0]
    //   0xE5[7:4] / 0xE6  -> dig_H5 [3:0] / [11:4]
    int16_t h4 = static_cast<int16_t>((static_cast<int16_t>(buf2[3]) << 4) |
                                       (buf2[4] & 0x0F));
    int16_t h5 = static_cast<int16_t>((static_cast<int16_t>(buf2[5]) >> 4) |
                                       (static_cast<int16_t>(buf2[6]) << 4));

    // Sign-extend from 12-bit two's complement (bugs here are exactly the
    // class of BME280 issue seen previously on the humidity path).
    if (h4 & 0x0800) h4 -= 4096;
    if (h5 & 0x0800) h5 -= 4096;

    calib_.dig_H4 = h4;
    calib_.dig_H5 = h5;

    // dig_H6: 0xE7, signed char
    calib_.dig_H6 = static_cast<int8_t>(buf2[6 + 1 - 1]); // placeholder removed below
    calib_.dig_H6 = static_cast<int8_t>(buf2[6]); // 0xE7 is buf2 index 6

    return true;
}

int32_t BME280Driver::compensateTemperature(int32_t adc_T)
{
    // datasheet §4.2.3, BME280_compensate_T_int32 (rev 1.1)
    int32_t var1, var2;
    var1 = ((((adc_T >> 3) - (static_cast<int32_t>(calib_.dig_T1) << 1))) *
             static_cast<int32_t>(calib_.dig_T2)) >> 11;
    var2 = (((((adc_T >> 4) - static_cast<int32_t>(calib_.dig_T1)) *
              ((adc_T >> 4) - static_cast<int32_t>(calib_.dig_T1))) >> 12) *
             static_cast<int32_t>(calib_.dig_T3)) >> 14;
    t_fine_ = var1 + var2;
    return (t_fine_ * 5 + 128) >> 8; // T in 0.01 degC units
}

uint32_t BME280Driver::compensatePressure(int32_t adc_P)
{
    // datasheet §4.2.3, BME280_compensate_P_int64 (rev 1.1)
    int64_t var1, var2, p;
    var1 = static_cast<int64_t>(t_fine_) - 128000;
    var2 = var1 * var1 * static_cast<int64_t>(calib_.dig_P6);
    var2 = var2 + ((var1 * static_cast<int64_t>(calib_.dig_P5)) << 17);
    var2 = var2 + (static_cast<int64_t>(calib_.dig_P4) << 35);
    var1 = ((var1 * var1 * static_cast<int64_t>(calib_.dig_P3)) >> 8) +
           ((var1 * static_cast<int64_t>(calib_.dig_P2)) << 12);
    var1 = ((static_cast<int64_t>(1) << 47) + var1) *
           static_cast<int64_t>(calib_.dig_P1) >> 33;

    if (var1 == 0)
    {
        return 0; // avoid divide-by-zero, per datasheet
    }

    p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (static_cast<int64_t>(calib_.dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (static_cast<int64_t>(calib_.dig_P8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (static_cast<int64_t>(calib_.dig_P7) << 4);

    return static_cast<uint32_t>(p); // Q24.8 format: divide by 256 for Pa
}

uint32_t BME280Driver::compensateHumidity(int32_t adc_H)
{
    // datasheet §4.2.3, bme280_compensate_H_int32 (rev 1.0)
    int32_t v_x1_u32r;

    v_x1_u32r = t_fine_ - static_cast<int32_t>(76800);

    v_x1_u32r = (((((adc_H << 14) -
                    (static_cast<int32_t>(calib_.dig_H4) << 20) -
                    (static_cast<int32_t>(calib_.dig_H5) * v_x1_u32r)) +
                   static_cast<int32_t>(16384)) >> 15) *
                 (((((((v_x1_u32r * static_cast<int32_t>(calib_.dig_H6)) >> 10) *
                      (((v_x1_u32r * static_cast<int32_t>(calib_.dig_H3)) >> 11) +
                       static_cast<int32_t>(32768))) >> 10) +
                    static_cast<int32_t>(2097152)) *
                   static_cast<int32_t>(calib_.dig_H2) + 8192) >> 14));

    v_x1_u32r = v_x1_u32r -
                (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) *
                  static_cast<int32_t>(calib_.dig_H1)) >> 4);

    v_x1_u32r = (v_x1_u32r < 0) ? 0 : v_x1_u32r;
    v_x1_u32r = (v_x1_u32r > 419430400) ? 419430400 : v_x1_u32r;

    return static_cast<uint32_t>(v_x1_u32r >> 12); // Q22.10 format: divide by 1024 for %RH
}

int32_t BME280Driver::readRaw()
{
    uint8_t buf[3] = {};
    if (!bus_.readRegs(REG_TEMP_MSB, buf, 3))
    {
        LOG_ERR("Temperature read failed");
        raw_ = INT32_MIN;
        return INT32_MIN;
    }
    raw_ = (static_cast<int32_t>(buf[0]) << 12) |
           (static_cast<int32_t>(buf[1]) << 4) |
           (static_cast<int32_t>(buf[2]) >> 4);
    return raw_;
}

float BME280Driver::toSI()
{
    if (raw_ == INT32_MIN || !calib_valid_)
        return NAN;
    int32_t T_centi = compensateTemperature(raw_); // 0.01 degC units
    return static_cast<float>(T_centi) / 100.0f;
}

uint8_t BME280Driver::deviceId()
{
    return bus_.address();
}

float BME280Driver::readTemperature_C()
{
    readRaw();
    return toSI();
}

float BME280Driver::readPressure_hPa()
{
    if (!calib_valid_)
        return NAN;

    uint8_t buf[3] = {};
    if (!bus_.readRegs(REG_PRESS_MSB, buf, 3))
    {
        LOG_ERR("Pressure read failed");
        return NAN;
    }
    int32_t raw_p = (static_cast<int32_t>(buf[0]) << 12) |
                    (static_cast<int32_t>(buf[1]) << 4) |
                    (static_cast<int32_t>(buf[2]) >> 4);

    // Temperature must be read first so t_fine_ is current, per datasheet
    // measurement flow (§3.4): temperature compensation feeds pressure/humidity.
    readRaw();
    compensateTemperature(raw_);

    uint32_t p_q24_8 = compensatePressure(raw_p);
    return static_cast<float>(p_q24_8) / 256.0f / 100.0f; // Pa -> hPa
}

float BME280Driver::readHumidity_pct()
{
    if (!calib_valid_)
        return NAN;

    uint8_t buf[2] = {};
    if (!bus_.readRegs(REG_HUM_MSB, buf, 2))
        return NAN;

    int32_t raw_h = (static_cast<int32_t>(buf[0]) << 8) | buf[1];

    // Needs a fresh t_fine_ too (see note in readPressure_hPa).
    int32_t raw_t = readRaw();
    compensateTemperature(raw_t);

    uint32_t h_q22_10 = compensateHumidity(raw_h);
    return static_cast<float>(h_q22_10) / 1024.0f;
}