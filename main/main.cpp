#include "esp_log.h"
#include "sdkconfig.h"

#include "tg28_i2c.h"
#include "tg28_pmic.h"

namespace {

constexpr char kTag[] = "TG28-ESP";

void log_startup_warning()
{
    ESP_LOGW(kTag, "首次联调请断开电池和所有下游负载，并使用限流电源接入 VBUS");
    ESP_LOGW(kTag, "确认寄存器回读和各路实测电压一致后，再逐路连接负载");
}

}  // namespace

extern "C" void app_main(void)
{
    log_startup_warning();
    ESP_ERROR_CHECK(tg28_i2c_init());

    tg28_snapshot_t snapshot = {};
    ESP_ERROR_CHECK(tg28_read_snapshot(&snapshot));
    tg28_log_snapshot(snapshot);

    if (tg28_matches_esp_defaults(snapshot)) {
        ESP_LOGI(kTag, "寄存器配置与 TG28-ESP 默认值一致");
    } else {
        ESP_LOGW(kTag, "回读值与 TG28-ESP 默认值不一致；保持负载断开并先核对实测电压");
    }

#if CONFIG_TG28_APPLY_ESP_CONFIGURATION
#if !CONFIG_TG28_CONFIRM_LOADS_DISCONNECTED
#error "TG28 writes require CONFIG_TG28_CONFIRM_LOADS_DISCONNECTED"
#endif
    ESP_LOGW(kTag, "写配置已启用：仅允许在电池和下游负载全部断开时执行");
    ESP_ERROR_CHECK(tg28_apply_esp_configuration(
        CONFIG_TG28_CHARGE_CURRENT_MA,
        CONFIG_TG28_CHARGE_VOLTAGE_MV));

    ESP_ERROR_CHECK(tg28_read_snapshot(&snapshot));
    tg28_log_snapshot(snapshot);
#else
    ESP_LOGI(kTag, "当前为只读模式，程序没有写入任何 TG28 寄存器");
#endif
}
