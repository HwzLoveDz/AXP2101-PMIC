#pragma once

#include <cstddef>
#include <cstdint>

#include "esp_err.h"

enum class tg28_rail_t : uint8_t {
    dcdc1, dcdc2, dcdc3, dcdc4,
    aldo1, aldo2, aldo3, aldo4,
    bldo1, bldo2, cpu_sldo, dldo1, dldo2,
};

enum class tg28_charge_state_t : uint8_t {
    trickle = 0, precharge = 1, constant_current = 2,
    constant_voltage = 3, done = 4, stopped = 5, unknown = 0xFF,
};

enum class tg28_battery_direction_t : uint8_t {
    standby = 0, charging = 1, discharging = 2, reserved = 3,
};

enum class tg28_watchdog_action_t : uint8_t {
    irq_only = 0, irq_and_reset = 1, irq_and_power_off = 2,
};

enum class tg28_charge_led_mode_t : uint8_t {
    off = 0, blink_1hz = 1, blink_4hz = 2, on = 3, charger_controlled = 4,
};

enum tg28_adc_channel_t : uint8_t {
    TG28_ADC_BATTERY = 1U << 0,
    TG28_ADC_TS = 1U << 1,
    TG28_ADC_VBUS = 1U << 2,
    TG28_ADC_VSYS = 1U << 3,
    TG28_ADC_DIE_TEMPERATURE = 1U << 4,
    TG28_ADC_GPADC = 1U << 5,
    TG28_ADC_ALL = 0x3F,
};

namespace tg28_irq {
constexpr uint32_t kBatteryWorkUnderTemperature = 1UL << 16;
constexpr uint32_t kBatteryWorkOverTemperature = 1UL << 17;
constexpr uint32_t kBatteryChargeUnderTemperature = 1UL << 18;
constexpr uint32_t kBatteryChargeOverTemperature = 1UL << 19;
constexpr uint32_t kNewSoc = 1UL << 20;
constexpr uint32_t kGaugeWatchdog = 1UL << 21;
constexpr uint32_t kLowBatteryWarning1 = 1UL << 22;
constexpr uint32_t kLowBatteryWarning2 = 1UL << 23;
constexpr uint32_t kPowerKeyPositive = 1UL << 8;
constexpr uint32_t kPowerKeyNegative = 1UL << 9;
constexpr uint32_t kPowerKeyLong = 1UL << 10;
constexpr uint32_t kPowerKeyShort = 1UL << 11;
constexpr uint32_t kBatteryRemove = 1UL << 12;
constexpr uint32_t kBatteryInsert = 1UL << 13;
constexpr uint32_t kVbusRemove = 1UL << 14;
constexpr uint32_t kVbusInsert = 1UL << 15;
constexpr uint32_t kBatteryOverVoltage = 1UL << 0;
constexpr uint32_t kChargeTimerExpired = 1UL << 1;
constexpr uint32_t kDieOverTemperature = 1UL << 2;
constexpr uint32_t kChargeStart = 1UL << 3;
constexpr uint32_t kChargeDone = 1UL << 4;
constexpr uint32_t kBatfetOverCurrent = 1UL << 5;
constexpr uint32_t kLdoOverCurrent = 1UL << 6;
constexpr uint32_t kWatchdogExpired = 1UL << 7;
constexpr uint32_t kAll = 0xFFFFFFUL;
}  // namespace tg28_irq

struct tg28_status_t {
    uint8_t raw_status1;
    uint8_t raw_status2;
    bool vbus_good;
    bool batfet_enabled;
    bool battery_present;
    bool battery_active;
    bool thermal_regulation;
    bool input_current_limited;
    bool system_powered;
    bool vbus_present;
    tg28_battery_direction_t battery_direction;
    tg28_charge_state_t charge_state;
};

struct tg28_rail_state_t {
    tg28_rail_t rail;
    bool enabled;
    uint16_t voltage_mv;
    uint8_t raw_voltage;
};

struct tg28_charger_settings_t {
    bool charger_enabled;
    bool gauge_enabled;
    bool battery_detection_enabled;
    bool termination_enabled;
    uint16_t precharge_current_ma;
    uint16_t constant_current_ma;
    uint16_t termination_current_ma;
    uint16_t target_voltage_mv;
    uint16_t input_voltage_limit_mv;
    uint16_t input_current_limit_ma;
};

struct tg28_telemetry_t {
    uint8_t enabled_channels;
    uint16_t battery_voltage_mv;
    uint16_t ts_voltage_mv;
    uint16_t vbus_voltage_mv;
    uint16_t system_voltage_mv;
    float die_temperature_c;
};

struct tg28_snapshot_t {
    uint8_t chip_id;
    tg28_status_t status;
    tg28_rail_state_t rails[13];
    tg28_charger_settings_t charger;
    uint8_t adc_enabled;
    int16_t battery_percent;
};

// Read-only diagnostics. None of these functions changes a TG28 register.
esp_err_t tg28_read_snapshot(tg28_snapshot_t *snapshot);
void tg28_log_snapshot(const tg28_snapshot_t &snapshot);
bool tg28_matches_esp_defaults(const tg28_snapshot_t &snapshot);
esp_err_t tg28_read_status(tg28_status_t *status);
esp_err_t tg28_get_rail_state(tg28_rail_t rail, tg28_rail_state_t *state);
esp_err_t tg28_get_charger_settings(tg28_charger_settings_t *settings);
esp_err_t tg28_read_telemetry(tg28_telemetry_t *telemetry);
esp_err_t tg28_get_battery_percent(int16_t *percent);
esp_err_t tg28_read_data_buffer(uint8_t index, uint8_t *value);

// Power-output control. Measure the selected rail before reconnecting a load.
esp_err_t tg28_set_rail_voltage(tg28_rail_t rail, uint16_t millivolts);
esp_err_t tg28_set_rail_enabled(tg28_rail_t rail, bool enabled);
esp_err_t tg28_set_dcdc_constant_current_mode(bool enabled);
esp_err_t tg28_set_dcdc_force_pwm(tg28_rail_t rail, bool enabled);

// Charger, power-path and battery settings.
esp_err_t tg28_set_charger_enabled(bool enabled);
esp_err_t tg28_set_gauge_enabled(bool enabled);
esp_err_t tg28_set_battery_detection_enabled(bool enabled);
esp_err_t tg28_set_precharge_current(uint16_t milliamps);
esp_err_t tg28_set_constant_charge_current(uint16_t milliamps);
esp_err_t tg28_set_termination_current(uint16_t milliamps, bool enabled);
esp_err_t tg28_set_charge_target_voltage(uint16_t millivolts);
esp_err_t tg28_set_charge_thermal_limit(uint16_t celsius);
esp_err_t tg28_set_charge_led_mode(tg28_charge_led_mode_t mode);
esp_err_t tg28_set_input_voltage_limit(uint16_t millivolts);
esp_err_t tg28_set_input_current_limit(uint16_t milliamps);
esp_err_t tg28_set_minimum_system_voltage(uint16_t millivolts);
esp_err_t tg28_set_power_off_voltage(uint16_t millivolts);
esp_err_t tg28_set_low_battery_thresholds(uint8_t warning_percent,
                                          uint8_t shutdown_percent);
esp_err_t tg28_set_backup_battery_voltage(uint16_t millivolts);
esp_err_t tg28_set_backup_battery_charger_enabled(bool enabled);
esp_err_t tg28_set_ts_pin_charge_control(bool enabled);

// ADC, watchdog and interrupt control. These functions write only when called.
esp_err_t tg28_set_adc_channels(uint8_t channel_mask, bool enabled);
esp_err_t tg28_configure_watchdog(uint16_t timeout_seconds,
                                  tg28_watchdog_action_t action);
esp_err_t tg28_set_watchdog_enabled(bool enabled);
esp_err_t tg28_feed_watchdog();
esp_err_t tg28_set_irq_enabled(uint32_t irq_mask, bool enabled);
esp_err_t tg28_read_irq_status(uint32_t *irq_status);
esp_err_t tg28_clear_irq_status(uint32_t irq_mask = tg28_irq::kAll);

// The caller supplies the 128-byte profile for the exact battery cell.
// Profile operations reset the gauge engine and never run by default.
esp_err_t tg28_write_battery_profile(const uint8_t *profile, size_t length,
                                     bool verify = true);
esp_err_t tg28_read_battery_profile(uint8_t *profile, size_t length);
esp_err_t tg28_write_data_buffer(uint8_t index, uint8_t value);

// Applies the documented TG28-ESP runtime settings. Call only with the battery
// and every downstream load disconnected. The default firmware never calls it.
esp_err_t tg28_apply_esp_configuration(uint16_t charge_current_ma,
                                       uint16_t charge_voltage_mv);
