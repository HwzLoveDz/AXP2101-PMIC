#include "tg28_pmic.h"

#include <array>

#include "esp_check.h"
#include "esp_log.h"
#include "tg28_i2c.h"

namespace {

constexpr char kTag[] = "TG28-PMIC";

constexpr uint8_t kExpectedChipId = 0x4A;
constexpr uint8_t kRegChipId = 0x03;
constexpr uint8_t kRegChargeCurrent = 0x62;
constexpr uint8_t kRegChargeVoltage = 0x64;
constexpr uint8_t kRegDcdcEnable = 0x80;
constexpr uint8_t kRegDcdc1Voltage = 0x82;
constexpr uint8_t kRegDcdc2Voltage = 0x83;
constexpr uint8_t kRegDcdc3Voltage = 0x84;
constexpr uint8_t kRegDcdc4Voltage = 0x85;
constexpr uint8_t kRegLdoEnable0 = 0x90;
constexpr uint8_t kRegLdoEnable1 = 0x91;

constexpr uint8_t kEspDcdcEnableMask = 0x09;
constexpr uint8_t kEspDcdc1VoltageRaw = 18;   // 1.5 V + 18 * 0.1 V = 3.3 V
constexpr uint8_t kEspDcdc4VoltageRaw = 100;  // 1.22 V + 29 * 0.02 V = 1.8 V
constexpr uint8_t kEspChargeCurrentRaw = 9;   // 300 mA
constexpr uint8_t kEspChargeVoltageRaw = 3;   // 4.2 V

esp_err_t read_register(uint8_t reg, uint8_t *value)
{
    const esp_err_t err = tg28_i2c_read_register(reg, value);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "read register 0x%02X failed: %s", reg, esp_err_to_name(err));
    }
    return err;
}

esp_err_t write_and_verify(uint8_t reg, uint8_t value, uint8_t mask)
{
    ESP_RETURN_ON_ERROR(tg28_i2c_write_register(reg, value), kTag,
                        "write register 0x%02X failed", reg);

    uint8_t readback = 0;
    ESP_RETURN_ON_ERROR(read_register(reg, &readback), kTag,
                        "verify register 0x%02X failed", reg);
    if ((readback & mask) != (value & mask)) {
        ESP_LOGE(kTag, "register 0x%02X verify mismatch: wrote 0x%02X, read 0x%02X",
                 reg, value, readback);
        return ESP_ERR_INVALID_RESPONSE;
    }
    return ESP_OK;
}

uint16_t decode_dcdc1_mv(uint8_t raw)
{
    const uint8_t code = raw & 0x1F;
    return code <= 19 ? static_cast<uint16_t>(1500 + code * 100) : 0;
}

uint16_t decode_piecewise_dcdc_mv(uint8_t raw, uint8_t max_code)
{
    const uint8_t code = raw & 0x7F;
    if (code <= 70) {
        return static_cast<uint16_t>(500 + code * 10);
    }
    if (code <= max_code) {
        return static_cast<uint16_t>(1220 + (code - 71) * 20);
    }
    return 0;
}

uint16_t decode_charge_current_ma(uint8_t raw)
{
    const uint8_t code = raw & 0x1F;
    if (code <= 8) {
        return static_cast<uint16_t>(code * 25);
    }
    if (code <= 21) {
        return static_cast<uint16_t>(200 + (code - 8) * 100);
    }
    return 0;
}

uint16_t decode_charge_voltage_mv(uint8_t raw)
{
    constexpr std::array<uint16_t, 8> table = {0, 4000, 4100, 4200, 4350, 4400, 0, 0};
    return table[raw & 0x07];
}

bool encode_charge_current_ma(uint16_t ma, uint8_t *raw)
{
    if (raw == nullptr) {
        return false;
    }
    if (ma <= 200 && ma % 25 == 0) {
        *raw = static_cast<uint8_t>(ma / 25);
        return true;
    }
    if (ma >= 300 && ma <= 1500 && ma % 100 == 0) {
        *raw = static_cast<uint8_t>(8 + (ma - 200) / 100);
        return true;
    }
    return false;
}

bool encode_charge_voltage_mv(uint16_t mv, uint8_t *raw)
{
    if (raw == nullptr) {
        return false;
    }
    switch (mv) {
    case 4000: *raw = 1; return true;
    case 4100: *raw = 2; return true;
    case 4200: *raw = 3; return true;
    case 4350: *raw = 4; return true;
    case 4400: *raw = 5; return true;
    default: return false;
    }
}

const char *enabled_text(bool enabled)
{
    return enabled ? "ON" : "OFF";
}

}  // namespace

esp_err_t tg28_read_snapshot(tg28_snapshot_t *snapshot)
{
    if (snapshot == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_RETURN_ON_ERROR(read_register(kRegChipId, &snapshot->chip_id), kTag, "chip ID");
    ESP_RETURN_ON_ERROR(read_register(kRegDcdcEnable, &snapshot->dcdc_enable), kTag, "DCDC enable");
    ESP_RETURN_ON_ERROR(read_register(kRegDcdc1Voltage, &snapshot->dcdc_voltage_raw[0]), kTag, "DCDC1 voltage");
    ESP_RETURN_ON_ERROR(read_register(kRegDcdc2Voltage, &snapshot->dcdc_voltage_raw[1]), kTag, "DCDC2 voltage");
    ESP_RETURN_ON_ERROR(read_register(kRegDcdc3Voltage, &snapshot->dcdc_voltage_raw[2]), kTag, "DCDC3 voltage");
    ESP_RETURN_ON_ERROR(read_register(kRegDcdc4Voltage, &snapshot->dcdc_voltage_raw[3]), kTag, "DCDC4 voltage");
    ESP_RETURN_ON_ERROR(read_register(kRegLdoEnable0, &snapshot->ldo_enable_0), kTag, "LDO enable 0");
    ESP_RETURN_ON_ERROR(read_register(kRegLdoEnable1, &snapshot->ldo_enable_1), kTag, "LDO enable 1");
    ESP_RETURN_ON_ERROR(read_register(kRegChargeCurrent, &snapshot->charge_current_raw), kTag, "charge current");
    ESP_RETURN_ON_ERROR(read_register(kRegChargeVoltage, &snapshot->charge_voltage_raw), kTag, "charge voltage");
    return ESP_OK;
}

void tg28_log_snapshot(const tg28_snapshot_t &snapshot)
{
    const uint16_t dcdc_mv[] = {
        decode_dcdc1_mv(snapshot.dcdc_voltage_raw[0]),
        decode_piecewise_dcdc_mv(snapshot.dcdc_voltage_raw[1], 87),
        decode_piecewise_dcdc_mv(snapshot.dcdc_voltage_raw[2], 87),
        decode_piecewise_dcdc_mv(snapshot.dcdc_voltage_raw[3], 102),
    };

    ESP_LOGI(kTag, "chip ID: 0x%02X%s", snapshot.chip_id,
             snapshot.chip_id == kExpectedChipId ? "" : " (unexpected)");
    for (int i = 0; i < 4; ++i) {
        ESP_LOGI(kTag, "DCDC%d: %s, configured=%u mV, raw=0x%02X",
                 i + 1,
                 enabled_text((snapshot.dcdc_enable & (1U << i)) != 0),
                 dcdc_mv[i],
                 snapshot.dcdc_voltage_raw[i]);
    }
    ESP_LOGI(kTag, "LDO enable: REG90=0x%02X REG91=0x%02X", snapshot.ldo_enable_0,
             snapshot.ldo_enable_1);
    ESP_LOGI(kTag, "charger: current=%u mA, target=%u mV",
             decode_charge_current_ma(snapshot.charge_current_raw),
             decode_charge_voltage_mv(snapshot.charge_voltage_raw));
    ESP_LOGI(kTag, "RTC-LDO1 is not switched by REG90/REG91; verify its 3.0 V output with a meter");
}

bool tg28_matches_esp_defaults(const tg28_snapshot_t &snapshot)
{
    return snapshot.chip_id == kExpectedChipId &&
           (snapshot.dcdc_enable & 0x0F) == kEspDcdcEnableMask &&
           (snapshot.dcdc_voltage_raw[0] & 0x1F) == kEspDcdc1VoltageRaw &&
           (snapshot.dcdc_voltage_raw[3] & 0x7F) == kEspDcdc4VoltageRaw &&
           snapshot.ldo_enable_0 == 0 &&
           (snapshot.ldo_enable_1 & 0x01) == 0 &&
           (snapshot.charge_current_raw & 0x1F) == kEspChargeCurrentRaw &&
           (snapshot.charge_voltage_raw & 0x07) == kEspChargeVoltageRaw;
}

esp_err_t tg28_apply_esp_configuration(uint16_t charge_current_ma,
                                       uint16_t charge_voltage_mv)
{
    uint8_t charge_current_raw = 0;
    uint8_t charge_voltage_raw = 0;
    if (!encode_charge_current_ma(charge_current_ma, &charge_current_raw)) {
        ESP_LOGE(kTag, "unsupported charge current: %u mA", charge_current_ma);
        return ESP_ERR_INVALID_ARG;
    }
    if (!encode_charge_voltage_mv(charge_voltage_mv, &charge_voltage_raw)) {
        ESP_LOGE(kTag, "unsupported charge voltage: %u mV", charge_voltage_mv);
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGW(kTag, "applying live configuration: DCDC1=3.3 V, DCDC4=1.8 V, charger=%u mA/%u mV",
             charge_current_ma, charge_voltage_mv);

    ESP_RETURN_ON_ERROR(write_and_verify(kRegDcdc1Voltage, kEspDcdc1VoltageRaw, 0x1F),
                        kTag, "DCDC1");
    ESP_RETURN_ON_ERROR(write_and_verify(kRegDcdc4Voltage, kEspDcdc4VoltageRaw, 0x7F),
                        kTag, "DCDC4");

    uint8_t current_reg = 0;
    ESP_RETURN_ON_ERROR(read_register(kRegChargeCurrent, &current_reg), kTag, "charge current read");
    current_reg = static_cast<uint8_t>((current_reg & 0xE0) | charge_current_raw);
    ESP_RETURN_ON_ERROR(write_and_verify(kRegChargeCurrent, current_reg, 0x1F), kTag, "charge current");

    uint8_t voltage_reg = 0;
    ESP_RETURN_ON_ERROR(read_register(kRegChargeVoltage, &voltage_reg), kTag, "charge voltage read");
    voltage_reg = static_cast<uint8_t>((voltage_reg & 0xF8) | charge_voltage_raw);
    ESP_RETURN_ON_ERROR(write_and_verify(kRegChargeVoltage, voltage_reg, 0x07), kTag, "charge voltage");

    ESP_RETURN_ON_ERROR(write_and_verify(kRegLdoEnable0, 0x00, 0xFF), kTag, "disable LDO outputs");

    uint8_t ldo_enable_1 = 0;
    ESP_RETURN_ON_ERROR(read_register(kRegLdoEnable1, &ldo_enable_1), kTag, "LDO enable 1 read");
    ldo_enable_1 &= static_cast<uint8_t>(~0x01U);
    ESP_RETURN_ON_ERROR(write_and_verify(kRegLdoEnable1, ldo_enable_1, 0x01), kTag, "disable DLDO2");

    uint8_t dcdc_enable = 0;
    ESP_RETURN_ON_ERROR(read_register(kRegDcdcEnable, &dcdc_enable), kTag, "DCDC enable read");
    dcdc_enable = static_cast<uint8_t>((dcdc_enable & 0xF0) | kEspDcdcEnableMask);
    ESP_RETURN_ON_ERROR(write_and_verify(kRegDcdcEnable, dcdc_enable, 0x0F), kTag, "DCDC enable");

    ESP_LOGI(kTag, "configuration write and register readback completed; measure every rail before reconnecting loads");
    return ESP_OK;
}
