#pragma once

#include <cstddef>
#include <cstdint>

#include "esp_err.h"

esp_err_t tg28_i2c_init();
esp_err_t tg28_i2c_read_register(uint8_t reg, uint8_t *value);
esp_err_t tg28_i2c_write_register(uint8_t reg, uint8_t value);
esp_err_t tg28_i2c_read(uint8_t reg, uint8_t *data, size_t length);
esp_err_t tg28_i2c_write(uint8_t reg, const uint8_t *data, size_t length);
esp_err_t tg28_i2c_update_bits(uint8_t reg, uint8_t mask, uint8_t value);
