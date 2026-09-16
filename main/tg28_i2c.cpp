#include "tg28_i2c.h"

#include <array>

#include "driver/i2c_master.h"
#include "esp_log.h"
#include "sdkconfig.h"

namespace {

constexpr char kTag[] = "TG28-I2C";
constexpr uint8_t kDeviceAddress = 0x34;
constexpr int kTimeoutMs = 1000;

i2c_master_bus_handle_t s_bus = nullptr;
i2c_master_dev_handle_t s_device = nullptr;

}  // namespace

esp_err_t tg28_i2c_init()
{
    i2c_master_bus_config_t bus_config = {};
    bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_config.i2c_port = static_cast<i2c_port_num_t>(CONFIG_TG28_I2C_PORT);
    bus_config.scl_io_num = static_cast<gpio_num_t>(CONFIG_TG28_I2C_SCL);
    bus_config.sda_io_num = static_cast<gpio_num_t>(CONFIG_TG28_I2C_SDA);
    bus_config.glitch_ignore_cnt = 7;
    bus_config.flags.enable_internal_pullup = true;

    esp_err_t err = i2c_new_master_bus(&bus_config, &s_bus);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "I2C bus init failed: %s", esp_err_to_name(err));
        return err;
    }

    i2c_device_config_t device_config = {};
    device_config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    device_config.device_address = kDeviceAddress;
    device_config.scl_speed_hz = CONFIG_TG28_I2C_FREQUENCY;

    err = i2c_master_bus_add_device(s_bus, &device_config, &s_device);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "TG28 device init failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(kTag, "I2C ready: port=%d SDA=%d SCL=%d speed=%d Hz address=0x%02X",
             CONFIG_TG28_I2C_PORT,
             CONFIG_TG28_I2C_SDA,
             CONFIG_TG28_I2C_SCL,
             CONFIG_TG28_I2C_FREQUENCY,
             kDeviceAddress);
    return ESP_OK;
}

esp_err_t tg28_i2c_read_register(uint8_t reg, uint8_t *value)
{
    return tg28_i2c_read(reg, value, 1);
}

esp_err_t tg28_i2c_write_register(uint8_t reg, uint8_t value)
{
    return tg28_i2c_write(reg, &value, 1);
}

esp_err_t tg28_i2c_read(uint8_t reg, uint8_t *data, size_t length)
{
    if (s_device == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    if (data == nullptr || length == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    return i2c_master_transmit_receive(s_device, &reg, 1, data, length, kTimeoutMs);
}

esp_err_t tg28_i2c_write(uint8_t reg, const uint8_t *data, size_t length)
{
    if (s_device == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    if (data == nullptr || length == 0 || length > 32) {
        return ESP_ERR_INVALID_ARG;
    }

    std::array<uint8_t, 33> payload = {};
    payload[0] = reg;
    for (size_t i = 0; i < length; ++i) {
        payload[i + 1] = data[i];
    }
    return i2c_master_transmit(s_device, payload.data(), length + 1, kTimeoutMs);
}

esp_err_t tg28_i2c_update_bits(uint8_t reg, uint8_t mask, uint8_t value)
{
    uint8_t current = 0;
    esp_err_t err = tg28_i2c_read_register(reg, &current);
    if (err != ESP_OK) {
        return err;
    }
    const uint8_t next = static_cast<uint8_t>((current & ~mask) | (value & mask));
    if (next == current) {
        return ESP_OK;
    }
    return tg28_i2c_write_register(reg, next);
}
