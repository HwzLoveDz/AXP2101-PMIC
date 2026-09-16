#include "tg28_pmic.h"

#include <array>
#include <cstring>

#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tg28_i2c.h"
#include "tg28_registers.h"

namespace {

constexpr char kTag[] = "TG28-PMIC";
constexpr size_t kRailCount = 13;
enum class voltage_encoding_t : uint8_t {
    dcdc1,
    dcdc_low_1540,
    dcdc_low_1840,
    ldo_100mv_3500,
    ldo_50mv_1400,
    ldo_100mv_3400,
};

struct rail_descriptor_t {
    tg28_rail_t rail;
    const char *name;
    uint8_t enable_register;
    uint8_t enable_mask;
    uint8_t voltage_register;
    uint8_t voltage_mask;
    voltage_encoding_t encoding;
};

constexpr std::array<rail_descriptor_t, kRailCount> kRails = {{
    {tg28_rail_t::dcdc1, "DCDC1", tg28_reg::kDcdcEnable, 1U << 0,
     tg28_reg::kDcdc1Voltage, 0x1F, voltage_encoding_t::dcdc1},
    {tg28_rail_t::dcdc2, "DCDC2", tg28_reg::kDcdcEnable, 1U << 1,
     tg28_reg::kDcdc2Voltage, 0x7F, voltage_encoding_t::dcdc_low_1540},
    {tg28_rail_t::dcdc3, "DCDC3", tg28_reg::kDcdcEnable, 1U << 2,
     tg28_reg::kDcdc3Voltage, 0x7F, voltage_encoding_t::dcdc_low_1540},
    {tg28_rail_t::dcdc4, "DCDC4", tg28_reg::kDcdcEnable, 1U << 3,
     tg28_reg::kDcdc4Voltage, 0x7F, voltage_encoding_t::dcdc_low_1840},
    {tg28_rail_t::aldo1, "ALDO1", tg28_reg::kLdoEnable1, 1U << 0,
     tg28_reg::kAldo1Voltage, 0x1F, voltage_encoding_t::ldo_100mv_3500},
    {tg28_rail_t::aldo2, "ALDO2", tg28_reg::kLdoEnable1, 1U << 1,
     tg28_reg::kAldo2Voltage, 0x1F, voltage_encoding_t::ldo_100mv_3500},
    {tg28_rail_t::aldo3, "ALDO3", tg28_reg::kLdoEnable1, 1U << 2,
     tg28_reg::kAldo3Voltage, 0x1F, voltage_encoding_t::ldo_100mv_3500},
    {tg28_rail_t::aldo4, "ALDO4", tg28_reg::kLdoEnable1, 1U << 3,
     tg28_reg::kAldo4Voltage, 0x1F, voltage_encoding_t::ldo_100mv_3500},
    {tg28_rail_t::bldo1, "BLDO1", tg28_reg::kLdoEnable1, 1U << 4,
     tg28_reg::kBldo1Voltage, 0x1F, voltage_encoding_t::ldo_100mv_3500},
    {tg28_rail_t::bldo2, "BLDO2", tg28_reg::kLdoEnable1, 1U << 5,
     tg28_reg::kBldo2Voltage, 0x1F, voltage_encoding_t::ldo_100mv_3500},
    {tg28_rail_t::cpu_sldo, "CPUSLDO", tg28_reg::kLdoEnable1, 1U << 6,
     tg28_reg::kCpuSldoVoltage, 0x1F, voltage_encoding_t::ldo_50mv_1400},
    {tg28_rail_t::dldo1, "DLDO1", tg28_reg::kLdoEnable1, 1U << 7,
     tg28_reg::kDldo1Voltage, 0x1F, voltage_encoding_t::ldo_100mv_3400},
    {tg28_rail_t::dldo2, "DLDO2", tg28_reg::kLdoEnable2, 1U << 0,
     tg28_reg::kDldo2Voltage, 0x1F, voltage_encoding_t::ldo_50mv_1400},
}};

const rail_descriptor_t *find_rail(tg28_rail_t rail)
{
    for (const auto &descriptor : kRails) {
        if (descriptor.rail == rail) {
            return &descriptor;
        }
    }
    return nullptr;
}

esp_err_t read_register(uint8_t reg, uint8_t *value)
{
    const esp_err_t err = tg28_i2c_read_register(reg, value);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "read register 0x%02X failed: %s", reg, esp_err_to_name(err));
    }
    return err;
}

esp_err_t update_and_verify(uint8_t reg, uint8_t mask, uint8_t value)
{
    ESP_RETURN_ON_ERROR(tg28_i2c_update_bits(reg, mask, value), kTag,
                        "write register 0x%02X failed", reg);
    uint8_t readback = 0;
    ESP_RETURN_ON_ERROR(read_register(reg, &readback), kTag,
                        "verify register 0x%02X failed", reg);
    if ((readback & mask) != (value & mask)) {
        ESP_LOGE(kTag, "register 0x%02X verify mismatch: expected 0x%02X, read 0x%02X",
                 reg, value & mask, readback & mask);
        return ESP_ERR_INVALID_RESPONSE;
    }
    return ESP_OK;
}

bool encode_voltage(voltage_encoding_t encoding, uint16_t mv, uint8_t *raw)
{
    if (raw == nullptr) {
        return false;
    }
    switch (encoding) {
    case voltage_encoding_t::dcdc1:
        if (mv >= 1500 && mv <= 3400 && (mv - 1500) % 100 == 0) {
            *raw = static_cast<uint8_t>((mv - 1500) / 100);
            return true;
        }
        break;
    case voltage_encoding_t::dcdc_low_1540:
    case voltage_encoding_t::dcdc_low_1840:
        if (mv >= 500 && mv <= 1200 && (mv - 500) % 10 == 0) {
            *raw = static_cast<uint8_t>((mv - 500) / 10);
            return true;
        }
        if (mv >= 1220 && (mv - 1220) % 20 == 0) {
            const uint16_t max_mv = encoding == voltage_encoding_t::dcdc_low_1540 ? 1540 : 1840;
            if (mv <= max_mv) {
                *raw = static_cast<uint8_t>(71 + (mv - 1220) / 20);
                return true;
            }
        }
        break;
    case voltage_encoding_t::ldo_100mv_3500:
        if (mv >= 500 && mv <= 3500 && (mv - 500) % 100 == 0) {
            *raw = static_cast<uint8_t>((mv - 500) / 100);
            return true;
        }
        break;
    case voltage_encoding_t::ldo_50mv_1400:
        if (mv >= 500 && mv <= 1400 && (mv - 500) % 50 == 0) {
            *raw = static_cast<uint8_t>((mv - 500) / 50);
            return true;
        }
        break;
    case voltage_encoding_t::ldo_100mv_3400:
        if (mv >= 500 && mv <= 3400 && (mv - 500) % 100 == 0) {
            *raw = static_cast<uint8_t>((mv - 500) / 100);
            return true;
        }
        break;
    }
    return false;
}

uint16_t decode_voltage(voltage_encoding_t encoding, uint8_t raw)
{
    switch (encoding) {
    case voltage_encoding_t::dcdc1: {
        const uint8_t code = raw & 0x1F;
        return code <= 19 ? static_cast<uint16_t>(1500 + code * 100) : 0;
    }
    case voltage_encoding_t::dcdc_low_1540:
    case voltage_encoding_t::dcdc_low_1840: {
        const uint8_t code = raw & 0x7F;
        const uint8_t max_code = encoding == voltage_encoding_t::dcdc_low_1540 ? 87 : 102;
        if (code <= 70) {
            return static_cast<uint16_t>(500 + code * 10);
        }
        return code <= max_code ? static_cast<uint16_t>(1220 + (code - 71) * 20) : 0;
    }
    case voltage_encoding_t::ldo_100mv_3500: {
        const uint8_t code = raw & 0x1F;
        return code <= 30 ? static_cast<uint16_t>(500 + code * 100) : 0;
    }
    case voltage_encoding_t::ldo_50mv_1400: {
        const uint8_t code = raw & 0x1F;
        return code <= 18 ? static_cast<uint16_t>(500 + code * 50) : 0;
    }
    case voltage_encoding_t::ldo_100mv_3400: {
        const uint8_t code = raw & 0x1F;
        return code <= 29 ? static_cast<uint16_t>(500 + code * 100) : 0;
    }
    }
    return 0;
}

bool encode_25ma(uint16_t ma, uint8_t *raw)
{
    if (raw != nullptr && ma <= 200 && ma % 25 == 0) {
        *raw = static_cast<uint8_t>(ma / 25);
        return true;
    }
    return false;
}

bool encode_charge_current(uint16_t ma, uint8_t *raw)
{
    if (encode_25ma(ma, raw)) {
        return true;
    }
    if (raw != nullptr && ma >= 300 && ma <= 1500 && ma % 100 == 0) {
        *raw = static_cast<uint8_t>(9 + (ma - 300) / 100);
        return true;
    }
    return false;
}

uint16_t decode_charge_current(uint8_t raw)
{
    const uint8_t code = raw & 0x1F;
    if (code <= 8) {
        return static_cast<uint16_t>(code * 25);
    }
    return code <= 21 ? static_cast<uint16_t>(300 + (code - 9) * 100) : 0;
}

bool encode_charge_voltage(uint16_t mv, uint8_t *raw)
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

uint16_t decode_charge_voltage(uint8_t raw)
{
    constexpr std::array<uint16_t, 8> table = {0, 4000, 4100, 4200, 4350, 4400, 0, 0};
    return table[raw & 0x07];
}

uint16_t decode_input_current(uint8_t raw)
{
    constexpr std::array<uint16_t, 8> table = {100, 500, 900, 1000, 1500, 2000, 0, 0};
    return table[raw & 0x07];
}

esp_err_t read_adc(uint8_t high_register, uint16_t *raw)
{
    if (raw == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t bytes[2] = {};
    ESP_RETURN_ON_ERROR(tg28_i2c_read(high_register, bytes, sizeof(bytes)), kTag,
                        "read ADC at 0x%02X", high_register);
    *raw = static_cast<uint16_t>(((bytes[0] & 0x3F) << 8) | bytes[1]);
    return ESP_OK;
}

esp_err_t set_boolean(uint8_t reg, uint8_t mask, bool enabled)
{
    return update_and_verify(reg, mask, enabled ? mask : 0);
}

esp_err_t pulse_fuel_gauge_reset()
{
    ESP_RETURN_ON_ERROR(tg28_i2c_update_bits(tg28_reg::kFuelGaugeReset, 1U << 2, 1U << 2),
                        kTag, "set fuel-gauge reset");
    vTaskDelay(pdMS_TO_TICKS(10));
    ESP_RETURN_ON_ERROR(tg28_i2c_update_bits(tg28_reg::kFuelGaugeReset, 1U << 2, 0),
                        kTag, "clear fuel-gauge reset");
    vTaskDelay(pdMS_TO_TICKS(10));
    return ESP_OK;
}

esp_err_t open_profile_stream()
{
    ESP_RETURN_ON_ERROR(tg28_i2c_update_bits(tg28_reg::kFuelGaugeControl, 0x11, 0),
                        kTag, "select profile ROM");
    vTaskDelay(pdMS_TO_TICKS(10));
    ESP_RETURN_ON_ERROR(tg28_i2c_update_bits(tg28_reg::kFuelGaugeControl, 0x11, 1),
                        kTag, "enable profile stream");
    vTaskDelay(pdMS_TO_TICKS(10));
    return ESP_OK;
}

esp_err_t close_profile_stream()
{
    const esp_err_t disable = tg28_i2c_update_bits(tg28_reg::kFuelGaugeControl, 0x01, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    const esp_err_t select_sram = tg28_i2c_update_bits(tg28_reg::kFuelGaugeControl, 0x10, 0x10);
    vTaskDelay(pdMS_TO_TICKS(10));
    return disable != ESP_OK ? disable : select_sram;
}

const char *rail_name(tg28_rail_t rail)
{
    const auto *descriptor = find_rail(rail);
    return descriptor == nullptr ? "UNKNOWN" : descriptor->name;
}

const char *enabled_text(bool enabled)
{
    return enabled ? "ON" : "OFF";
}

const char *direction_text(tg28_battery_direction_t direction)
{
    switch (direction) {
    case tg28_battery_direction_t::standby: return "standby";
    case tg28_battery_direction_t::charging: return "charging";
    case tg28_battery_direction_t::discharging: return "discharging";
    default: return "reserved";
    }
}

const char *charge_state_text(tg28_charge_state_t state)
{
    switch (state) {
    case tg28_charge_state_t::trickle: return "trickle";
    case tg28_charge_state_t::precharge: return "precharge";
    case tg28_charge_state_t::constant_current: return "constant-current";
    case tg28_charge_state_t::constant_voltage: return "constant-voltage";
    case tg28_charge_state_t::done: return "done";
    case tg28_charge_state_t::stopped: return "stopped";
    default: return "unknown";
    }
}

}  // namespace

esp_err_t tg28_read_status(tg28_status_t *status)
{
    if (status == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    ESP_RETURN_ON_ERROR(read_register(tg28_reg::kStatus1, &status->raw_status1), kTag, "status1");
    ESP_RETURN_ON_ERROR(read_register(tg28_reg::kStatus2, &status->raw_status2), kTag, "status2");
    status->vbus_good = (status->raw_status1 & (1U << 5)) != 0;
    status->batfet_enabled = (status->raw_status1 & (1U << 4)) != 0;
    status->battery_present = (status->raw_status1 & (1U << 3)) != 0;
    status->battery_active = (status->raw_status1 & (1U << 2)) != 0;
    status->thermal_regulation = (status->raw_status1 & (1U << 1)) != 0;
    status->input_current_limited = (status->raw_status1 & 1U) != 0;
    status->battery_direction = static_cast<tg28_battery_direction_t>((status->raw_status2 >> 5) & 0x03);
    status->system_powered = (status->raw_status2 & (1U << 4)) != 0;
    status->vbus_present = status->vbus_good && (status->raw_status2 & (1U << 3)) == 0;
    const uint8_t charge = status->raw_status2 & 0x07;
    status->charge_state = charge <= 5 ? static_cast<tg28_charge_state_t>(charge)
                                       : tg28_charge_state_t::unknown;
    return ESP_OK;
}

esp_err_t tg28_get_rail_state(tg28_rail_t rail, tg28_rail_state_t *state)
{
    if (state == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    const auto *descriptor = find_rail(rail);
    if (descriptor == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t enable = 0;
    ESP_RETURN_ON_ERROR(read_register(descriptor->enable_register, &enable), kTag, "rail enable");
    ESP_RETURN_ON_ERROR(read_register(descriptor->voltage_register, &state->raw_voltage), kTag,
                        "rail voltage");
    state->rail = rail;
    state->enabled = (enable & descriptor->enable_mask) != 0;
    state->voltage_mv = decode_voltage(descriptor->encoding, state->raw_voltage);
    return ESP_OK;
}

esp_err_t tg28_set_rail_voltage(tg28_rail_t rail, uint16_t millivolts)
{
    const auto *descriptor = find_rail(rail);
    uint8_t raw = 0;
    if (descriptor == nullptr || !encode_voltage(descriptor->encoding, millivolts, &raw)) {
        ESP_LOGE(kTag, "unsupported %s voltage: %u mV", rail_name(rail), millivolts);
        return ESP_ERR_INVALID_ARG;
    }
    return update_and_verify(descriptor->voltage_register, descriptor->voltage_mask, raw);
}

esp_err_t tg28_set_rail_enabled(tg28_rail_t rail, bool enabled)
{
    const auto *descriptor = find_rail(rail);
    if (descriptor == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    return set_boolean(descriptor->enable_register, descriptor->enable_mask, enabled);
}

esp_err_t tg28_set_dcdc_constant_current_mode(bool enabled)
{
    return set_boolean(tg28_reg::kDcdcEnable, 1U << 6, enabled);
}

esp_err_t tg28_set_dcdc_force_pwm(tg28_rail_t rail, bool enabled)
{
    const uint8_t index = static_cast<uint8_t>(rail);
    if (index > static_cast<uint8_t>(tg28_rail_t::dcdc4)) {
        return ESP_ERR_INVALID_ARG;
    }
    return set_boolean(tg28_reg::kDcdcMode, static_cast<uint8_t>(1U << (index + 2)), enabled);
}

esp_err_t tg28_get_charger_settings(tg28_charger_settings_t *settings)
{
    if (settings == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t control = 0, detection = 0, precharge = 0, current = 0;
    uint8_t termination = 0, target = 0, input_voltage = 0, input_current = 0;
    ESP_RETURN_ON_ERROR(read_register(tg28_reg::kChargeGaugeWatchdogControl, &control), kTag, "charger control");
    ESP_RETURN_ON_ERROR(read_register(tg28_reg::kBatteryDetection, &detection), kTag, "battery detection");
    ESP_RETURN_ON_ERROR(read_register(tg28_reg::kPrechargeCurrent, &precharge), kTag, "precharge current");
    ESP_RETURN_ON_ERROR(read_register(tg28_reg::kConstantChargeCurrent, &current), kTag, "charge current");
    ESP_RETURN_ON_ERROR(read_register(tg28_reg::kTerminationCurrent, &termination), kTag, "termination current");
    ESP_RETURN_ON_ERROR(read_register(tg28_reg::kChargeTargetVoltage, &target), kTag, "charge voltage");
    ESP_RETURN_ON_ERROR(read_register(tg28_reg::kInputVoltageLimit, &input_voltage), kTag, "input voltage limit");
    ESP_RETURN_ON_ERROR(read_register(tg28_reg::kInputCurrentLimit, &input_current), kTag, "input current limit");
    settings->charger_enabled = (control & (1U << 1)) != 0;
    settings->gauge_enabled = (control & (1U << 3)) != 0;
    settings->battery_detection_enabled = (detection & 1U) != 0;
    settings->termination_enabled = (termination & (1U << 4)) != 0;
    settings->precharge_current_ma = static_cast<uint16_t>((precharge & 0x0F) * 25);
    settings->constant_current_ma = decode_charge_current(current);
    settings->termination_current_ma = static_cast<uint16_t>((termination & 0x0F) * 25);
    settings->target_voltage_mv = decode_charge_voltage(target);
    settings->input_voltage_limit_mv = static_cast<uint16_t>(3880 + (input_voltage & 0x0F) * 80);
    settings->input_current_limit_ma = decode_input_current(input_current);
    return ESP_OK;
}

esp_err_t tg28_set_charger_enabled(bool enabled)
{
    return set_boolean(tg28_reg::kChargeGaugeWatchdogControl, 1U << 1, enabled);
}

esp_err_t tg28_set_gauge_enabled(bool enabled)
{
    return set_boolean(tg28_reg::kChargeGaugeWatchdogControl, 1U << 3, enabled);
}

esp_err_t tg28_set_battery_detection_enabled(bool enabled)
{
    return set_boolean(tg28_reg::kBatteryDetection, 1U, enabled);
}

esp_err_t tg28_set_precharge_current(uint16_t milliamps)
{
    uint8_t raw = 0;
    return encode_25ma(milliamps, &raw)
               ? update_and_verify(tg28_reg::kPrechargeCurrent, 0x0F, raw)
               : ESP_ERR_INVALID_ARG;
}

esp_err_t tg28_set_constant_charge_current(uint16_t milliamps)
{
    uint8_t raw = 0;
    return encode_charge_current(milliamps, &raw)
               ? update_and_verify(tg28_reg::kConstantChargeCurrent, 0x1F, raw)
               : ESP_ERR_INVALID_ARG;
}

esp_err_t tg28_set_termination_current(uint16_t milliamps, bool enabled)
{
    uint8_t raw = 0;
    if (!encode_25ma(milliamps, &raw)) {
        return ESP_ERR_INVALID_ARG;
    }
    return update_and_verify(tg28_reg::kTerminationCurrent, 0x1F,
                             static_cast<uint8_t>(raw | (enabled ? 0x10 : 0)));
}

esp_err_t tg28_set_charge_target_voltage(uint16_t millivolts)
{
    uint8_t raw = 0;
    return encode_charge_voltage(millivolts, &raw)
               ? update_and_verify(tg28_reg::kChargeTargetVoltage, 0x07, raw)
               : ESP_ERR_INVALID_ARG;
}

esp_err_t tg28_set_charge_thermal_limit(uint16_t celsius)
{
    uint8_t raw = 0;
    switch (celsius) {
    case 60: raw = 0; break;
    case 80: raw = 1; break;
    case 100: raw = 2; break;
    case 120: raw = 3; break;
    default: return ESP_ERR_INVALID_ARG;
    }
    return update_and_verify(tg28_reg::kThermalRegulation, 0x03, raw);
}

esp_err_t tg28_set_charge_led_mode(tg28_charge_led_mode_t mode)
{
    if (mode == tg28_charge_led_mode_t::charger_controlled) {
        return update_and_verify(tg28_reg::kChargeLedControl, 0x07, 0x01);
    }
    const uint8_t raw_mode = static_cast<uint8_t>(mode);
    if (raw_mode > static_cast<uint8_t>(tg28_charge_led_mode_t::on)) {
        return ESP_ERR_INVALID_ARG;
    }
    return update_and_verify(tg28_reg::kChargeLedControl, 0x37,
                             static_cast<uint8_t>(0x05 | (raw_mode << 4)));
}

esp_err_t tg28_set_input_voltage_limit(uint16_t millivolts)
{
    if (millivolts < 3880 || millivolts > 5080 || (millivolts - 3880) % 80 != 0) {
        return ESP_ERR_INVALID_ARG;
    }
    return update_and_verify(tg28_reg::kInputVoltageLimit, 0x0F,
                             static_cast<uint8_t>((millivolts - 3880) / 80));
}

esp_err_t tg28_set_input_current_limit(uint16_t milliamps)
{
    constexpr std::array<uint16_t, 6> values = {100, 500, 900, 1000, 1500, 2000};
    for (uint8_t i = 0; i < values.size(); ++i) {
        if (values[i] == milliamps) {
            return update_and_verify(tg28_reg::kInputCurrentLimit, 0x07, i);
        }
    }
    return ESP_ERR_INVALID_ARG;
}

esp_err_t tg28_set_minimum_system_voltage(uint16_t millivolts)
{
    if (millivolts < 3200 || millivolts > 3900 || (millivolts - 3200) % 100 != 0) {
        return ESP_ERR_INVALID_ARG;
    }
    return update_and_verify(tg28_reg::kMinimumSystemVoltage, 0x07,
                             static_cast<uint8_t>((millivolts - 3200) / 100));
}

esp_err_t tg28_set_power_off_voltage(uint16_t millivolts)
{
    if (millivolts < 2600 || millivolts > 3300 || (millivolts - 2600) % 100 != 0) {
        return ESP_ERR_INVALID_ARG;
    }
    return update_and_verify(tg28_reg::kPowerOffVoltage, 0x07,
                             static_cast<uint8_t>((millivolts - 2600) / 100));
}

esp_err_t tg28_set_low_battery_thresholds(uint8_t warning_percent, uint8_t shutdown_percent)
{
    if (warning_percent < 5 || warning_percent > 20 || shutdown_percent > 15) {
        return ESP_ERR_INVALID_ARG;
    }
    const uint8_t raw = static_cast<uint8_t>(((warning_percent - 5) << 4) | shutdown_percent);
    return update_and_verify(tg28_reg::kLowBatteryWarning, 0xFF, raw);
}

esp_err_t tg28_set_backup_battery_voltage(uint16_t millivolts)
{
    if (millivolts < 2600 || millivolts > 3300 || (millivolts - 2600) % 100 != 0) {
        return ESP_ERR_INVALID_ARG;
    }
    return update_and_verify(tg28_reg::kBackupBatteryVoltage, 0x07,
                             static_cast<uint8_t>((millivolts - 2600) / 100));
}

esp_err_t tg28_set_backup_battery_charger_enabled(bool enabled)
{
    return set_boolean(tg28_reg::kChargeGaugeWatchdogControl, 1U << 2, enabled);
}

esp_err_t tg28_set_ts_pin_charge_control(bool enabled)
{
    return update_and_verify(tg28_reg::kTsPinControl, 1U << 4, enabled ? 0 : 1U << 4);
}

esp_err_t tg28_set_adc_channels(uint8_t channel_mask, bool enabled)
{
    if ((channel_mask & ~TG28_ADC_ALL) != 0) {
        return ESP_ERR_INVALID_ARG;
    }
    return update_and_verify(tg28_reg::kAdcChannelEnable, channel_mask,
                             enabled ? channel_mask : 0);
}

esp_err_t tg28_read_telemetry(tg28_telemetry_t *telemetry)
{
    if (telemetry == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    *telemetry = {};
    ESP_RETURN_ON_ERROR(read_register(tg28_reg::kAdcChannelEnable, &telemetry->enabled_channels),
                        kTag, "ADC channels");
    uint16_t raw = 0;
    if ((telemetry->enabled_channels & TG28_ADC_BATTERY) != 0) {
        ESP_RETURN_ON_ERROR(read_adc(tg28_reg::kBatteryVoltageHigh, &raw), kTag, "battery ADC");
        telemetry->battery_voltage_mv = raw;
    }
    if ((telemetry->enabled_channels & TG28_ADC_TS) != 0) {
        ESP_RETURN_ON_ERROR(read_adc(tg28_reg::kTsVoltageHigh, &raw), kTag, "TS ADC");
        telemetry->ts_voltage_mv = static_cast<uint16_t>(raw / 2);
    }
    if ((telemetry->enabled_channels & TG28_ADC_VBUS) != 0) {
        ESP_RETURN_ON_ERROR(read_adc(tg28_reg::kVbusVoltageHigh, &raw), kTag, "VBUS ADC");
        telemetry->vbus_voltage_mv = raw;
    }
    if ((telemetry->enabled_channels & TG28_ADC_VSYS) != 0) {
        ESP_RETURN_ON_ERROR(read_adc(tg28_reg::kSystemVoltageHigh, &raw), kTag, "VSYS ADC");
        telemetry->system_voltage_mv = raw;
    }
    if ((telemetry->enabled_channels & TG28_ADC_DIE_TEMPERATURE) != 0) {
        ESP_RETURN_ON_ERROR(read_adc(tg28_reg::kDieTemperatureHigh, &raw), kTag, "temperature ADC");
        telemetry->die_temperature_c = 22.0F + (7274.0F - raw) / 20.0F;
    }
    return ESP_OK;
}

esp_err_t tg28_get_battery_percent(int16_t *percent)
{
    if (percent == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    tg28_status_t status = {};
    ESP_RETURN_ON_ERROR(tg28_read_status(&status), kTag, "battery status");
    if (!status.battery_present) {
        *percent = -1;
        return ESP_OK;
    }
    uint8_t raw = 0;
    ESP_RETURN_ON_ERROR(read_register(tg28_reg::kBatteryPercent, &raw), kTag, "battery percent");
    *percent = raw <= 100 ? raw : -1;
    return ESP_OK;
}

esp_err_t tg28_configure_watchdog(uint16_t timeout_seconds, tg28_watchdog_action_t action)
{
    uint8_t timeout_code = 0;
    uint16_t value = 1;
    while (value < timeout_seconds && timeout_code < 7) {
        value <<= 1;
        ++timeout_code;
    }
    if (value != timeout_seconds || static_cast<uint8_t>(action) > 2) {
        return ESP_ERR_INVALID_ARG;
    }
    return update_and_verify(tg28_reg::kWatchdogControl, 0x37,
                             static_cast<uint8_t>((static_cast<uint8_t>(action) << 4) | timeout_code));
}

esp_err_t tg28_set_watchdog_enabled(bool enabled)
{
    return set_boolean(tg28_reg::kChargeGaugeWatchdogControl, 1U, enabled);
}

esp_err_t tg28_feed_watchdog()
{
    return tg28_i2c_update_bits(tg28_reg::kWatchdogControl, 1U << 3, 1U << 3);
}

esp_err_t tg28_set_irq_enabled(uint32_t irq_mask, bool enabled)
{
    if ((irq_mask & ~tg28_irq::kAll) != 0) {
        return ESP_ERR_INVALID_ARG;
    }
    const uint8_t bytes[] = {
        static_cast<uint8_t>(irq_mask >> 16),
        static_cast<uint8_t>(irq_mask >> 8),
        static_cast<uint8_t>(irq_mask),
    };
    for (size_t i = 0; i < 3; ++i) {
        ESP_RETURN_ON_ERROR(update_and_verify(static_cast<uint8_t>(tg28_reg::kIrqEnable1 + i),
                                              bytes[i], enabled ? bytes[i] : 0),
                            kTag, "IRQ enable register");
    }
    return ESP_OK;
}

esp_err_t tg28_read_irq_status(uint32_t *irq_status)
{
    if (irq_status == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t bytes[3] = {};
    ESP_RETURN_ON_ERROR(tg28_i2c_read(tg28_reg::kIrqStatus1, bytes, sizeof(bytes)), kTag,
                        "read IRQ status");
    *irq_status = (static_cast<uint32_t>(bytes[0]) << 16) |
                  (static_cast<uint32_t>(bytes[1]) << 8) | bytes[2];
    return ESP_OK;
}

esp_err_t tg28_clear_irq_status(uint32_t irq_mask)
{
    if ((irq_mask & ~tg28_irq::kAll) != 0) {
        return ESP_ERR_INVALID_ARG;
    }
    const uint8_t bytes[] = {
        static_cast<uint8_t>(irq_mask >> 16),
        static_cast<uint8_t>(irq_mask >> 8),
        static_cast<uint8_t>(irq_mask),
    };
    return tg28_i2c_write(tg28_reg::kIrqStatus1, bytes, sizeof(bytes));
}

esp_err_t tg28_read_data_buffer(uint8_t index, uint8_t *value)
{
    if (index >= 4 || value == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    return read_register(static_cast<uint8_t>(tg28_reg::kDataBuffer0 + index), value);
}

esp_err_t tg28_write_data_buffer(uint8_t index, uint8_t value)
{
    if (index >= 4) {
        return ESP_ERR_INVALID_ARG;
    }
    return update_and_verify(static_cast<uint8_t>(tg28_reg::kDataBuffer0 + index), 0xFF, value);
}

esp_err_t tg28_read_battery_profile(uint8_t *profile, size_t length)
{
    if (profile == nullptr || length != tg28_reg::kBatteryProfileSize) {
        return ESP_ERR_INVALID_ARG;
    }
    ESP_RETURN_ON_ERROR(open_profile_stream(), kTag, "open battery profile");
    esp_err_t result = ESP_OK;
    for (size_t i = 0; i < length; ++i) {
        result = read_register(tg28_reg::kBatteryParameter, &profile[i]);
        if (result != ESP_OK) {
            break;
        }
    }
    const esp_err_t close_result = close_profile_stream();
    return result != ESP_OK ? result : close_result;
}

esp_err_t tg28_write_battery_profile(const uint8_t *profile, size_t length, bool verify)
{
    if (profile == nullptr || length != tg28_reg::kBatteryProfileSize) {
        return ESP_ERR_INVALID_ARG;
    }
    ESP_RETURN_ON_ERROR(pulse_fuel_gauge_reset(), kTag, "reset fuel gauge");
    ESP_RETURN_ON_ERROR(open_profile_stream(), kTag, "open battery profile");
    esp_err_t result = ESP_OK;
    for (size_t i = 0; i < length; ++i) {
        result = tg28_i2c_write_register(tg28_reg::kBatteryParameter, profile[i]);
        if (result != ESP_OK) {
            break;
        }
    }
    if (result == ESP_OK && verify) {
        result = tg28_i2c_update_bits(tg28_reg::kFuelGaugeControl, 0x01, 0);
        vTaskDelay(pdMS_TO_TICKS(10));
        if (result == ESP_OK) {
            result = tg28_i2c_update_bits(tg28_reg::kFuelGaugeControl, 0x01, 1);
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        for (size_t i = 0; result == ESP_OK && i < length; ++i) {
            uint8_t readback = 0;
            result = read_register(tg28_reg::kBatteryParameter, &readback);
            if (result == ESP_OK && readback != profile[i]) {
                ESP_LOGE(kTag, "battery profile verify mismatch at byte %u", static_cast<unsigned>(i));
                result = ESP_ERR_INVALID_RESPONSE;
            }
        }
    }
    const esp_err_t close_result = close_profile_stream();
    if (result == ESP_OK) {
        result = close_result;
    }
    if (result == ESP_OK) {
        result = pulse_fuel_gauge_reset();
    }
    return result;
}

esp_err_t tg28_read_snapshot(tg28_snapshot_t *snapshot)
{
    if (snapshot == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    *snapshot = {};
    ESP_RETURN_ON_ERROR(read_register(tg28_reg::kChipId, &snapshot->chip_id), kTag, "chip ID");
    ESP_RETURN_ON_ERROR(tg28_read_status(&snapshot->status), kTag, "status");
    for (size_t i = 0; i < kRailCount; ++i) {
        ESP_RETURN_ON_ERROR(tg28_get_rail_state(kRails[i].rail, &snapshot->rails[i]), kTag,
                            "rail state");
    }
    ESP_RETURN_ON_ERROR(tg28_get_charger_settings(&snapshot->charger), kTag, "charger settings");
    ESP_RETURN_ON_ERROR(read_register(tg28_reg::kAdcChannelEnable, &snapshot->adc_enabled), kTag,
                        "ADC channel state");
    ESP_RETURN_ON_ERROR(tg28_get_battery_percent(&snapshot->battery_percent), kTag,
                        "battery percent");
    return ESP_OK;
}

void tg28_log_snapshot(const tg28_snapshot_t &snapshot)
{
    ESP_LOGI(kTag, "chip ID: 0x%02X%s", snapshot.chip_id,
             snapshot.chip_id == tg28_reg::kExpectedChipId ? "" : " (unexpected)");
    ESP_LOGI(kTag, "system: VBUS=%s/%s battery=%s BATFET=%s direction=%s charge=%s",
             snapshot.status.vbus_present ? "present" : "absent",
             snapshot.status.vbus_good ? "good" : "not-good",
             snapshot.status.battery_present ? "present" : "absent",
             enabled_text(snapshot.status.batfet_enabled),
             direction_text(snapshot.status.battery_direction),
             charge_state_text(snapshot.status.charge_state));
    for (const auto &rail : snapshot.rails) {
        ESP_LOGI(kTag, "%-7s %s configured=%u mV raw=0x%02X", rail_name(rail.rail),
                 enabled_text(rail.enabled), rail.voltage_mv, rail.raw_voltage);
    }
    ESP_LOGI(kTag, "charger: %s, gauge=%s, detection=%s, precharge=%u mA, CC=%u mA, CV=%u mV, ITERM=%u mA/%s",
             enabled_text(snapshot.charger.charger_enabled),
             enabled_text(snapshot.charger.gauge_enabled),
             enabled_text(snapshot.charger.battery_detection_enabled),
             snapshot.charger.precharge_current_ma,
             snapshot.charger.constant_current_ma,
             snapshot.charger.target_voltage_mv,
             snapshot.charger.termination_current_ma,
             enabled_text(snapshot.charger.termination_enabled));
    ESP_LOGI(kTag, "input limits: %u mV / %u mA; ADC enable=0x%02X; battery=%d%%",
             snapshot.charger.input_voltage_limit_mv,
             snapshot.charger.input_current_limit_ma,
             snapshot.adc_enabled, snapshot.battery_percent);
    ESP_LOGI(kTag, "RTC-LDO1 is not controlled by REG90/REG91; confirm its 3.0 V output with a meter");
}

bool tg28_matches_esp_defaults(const tg28_snapshot_t &snapshot)
{
    return snapshot.chip_id == tg28_reg::kExpectedChipId &&
           snapshot.rails[0].enabled && snapshot.rails[0].voltage_mv == 3300 &&
           !snapshot.rails[1].enabled && !snapshot.rails[2].enabled &&
           snapshot.rails[3].enabled && snapshot.rails[3].voltage_mv == 1800 &&
           !snapshot.rails[4].enabled && !snapshot.rails[5].enabled &&
           !snapshot.rails[6].enabled && !snapshot.rails[7].enabled &&
           !snapshot.rails[8].enabled && !snapshot.rails[9].enabled &&
           !snapshot.rails[10].enabled && !snapshot.rails[11].enabled &&
           !snapshot.rails[12].enabled &&
           snapshot.charger.constant_current_ma == 300 &&
           snapshot.charger.target_voltage_mv == 4200;
}

esp_err_t tg28_apply_esp_configuration(uint16_t charge_current_ma, uint16_t charge_voltage_mv)
{
    uint8_t current_raw = 0;
    uint8_t voltage_raw = 0;
    if (!encode_charge_current(charge_current_ma, &current_raw) ||
        !encode_charge_voltage(charge_voltage_mv, &voltage_raw)) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGW(kTag, "writing live TG28-ESP settings; battery and every load must be disconnected");
    ESP_RETURN_ON_ERROR(tg28_set_rail_voltage(tg28_rail_t::dcdc1, 3300), kTag, "DCDC1 voltage");
    ESP_RETURN_ON_ERROR(tg28_set_rail_voltage(tg28_rail_t::dcdc4, 1800), kTag, "DCDC4 voltage");
    ESP_RETURN_ON_ERROR(tg28_set_constant_charge_current(charge_current_ma), kTag, "charge current");
    ESP_RETURN_ON_ERROR(tg28_set_charge_target_voltage(charge_voltage_mv), kTag, "charge voltage");
    for (size_t i = 1; i < kRailCount; ++i) {
        if (kRails[i].rail != tg28_rail_t::dcdc4) {
            ESP_RETURN_ON_ERROR(tg28_set_rail_enabled(kRails[i].rail, false), kTag,
                                "disable unused rail");
        }
    }
    ESP_RETURN_ON_ERROR(tg28_set_rail_enabled(tg28_rail_t::dcdc1, true), kTag, "enable DCDC1");
    ESP_RETURN_ON_ERROR(tg28_set_rail_enabled(tg28_rail_t::dcdc4, true), kTag, "enable DCDC4");
    ESP_LOGI(kTag, "TG28-ESP runtime settings verified; measure every rail before reconnecting loads");
    return ESP_OK;
}
