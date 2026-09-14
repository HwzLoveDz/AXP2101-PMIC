#pragma once

#include <cstdint>

#include "esp_err.h"

struct tg28_snapshot_t {
    uint8_t chip_id;
    uint8_t dcdc_enable;
    uint8_t dcdc_voltage_raw[4];
    uint8_t ldo_enable_0;
    uint8_t ldo_enable_1;
    uint8_t charge_current_raw;
    uint8_t charge_voltage_raw;
};

esp_err_t tg28_read_snapshot(tg28_snapshot_t *snapshot);
void tg28_log_snapshot(const tg28_snapshot_t &snapshot);
bool tg28_matches_esp_defaults(const tg28_snapshot_t &snapshot);

// This function changes live power rails. Call it only with the battery and all
// downstream loads disconnected. The default firmware never calls it.
esp_err_t tg28_apply_esp_configuration(uint16_t charge_current_ma,
                                       uint16_t charge_voltage_mv);
