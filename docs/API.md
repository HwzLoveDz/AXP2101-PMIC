# TG28 驱动接口

所有公开接口位于 `main/tg28_pmic.h`。初始化 I²C 后即可调用读取接口；任何 `set`、`write`、`clear`、`feed` 或 `apply` 接口都可能写寄存器，应由应用程序按已验证的电源时序显式调用。

## 读取与诊断

| 接口 | 用途 |
| --- | --- |
| `tg28_read_snapshot()` | 一次读取芯片 ID、系统状态、13 路电源、充电设置、ADC 开关与 SOC |
| `tg28_read_status()` | 读取 VBUS、电池、BATFET、充放电方向和充电阶段 |
| `tg28_get_rail_state()` | 读取单路电源开关与配置电压 |
| `tg28_get_charger_settings()` | 读取充电使能、电流、电压、终止和输入限值 |
| `tg28_read_telemetry()` | 读取已开启的 VBAT、VBUS、VSYS、TS 与芯片温度 ADC |
| `tg28_get_battery_percent()` | 读取电量计 SOC；未检测到电池时返回 `-1` |
| `tg28_read_irq_status()` | 读取 24 位 IRQ 状态，不自动清除 |

`tg28_read_snapshot()` 与 `tg28_log_snapshot()` 是默认示例使用的只读路径，不会为了读取数据而开启 ADC、IRQ 或电量计。

## 电源轨

`tg28_rail_t` 覆盖 DCDC1～4、ALDO1～4、BLDO1～2、CPUSLDO、DLDO1～2。

```cpp
// 只在确认下游负载允许 3.3 V 后调用。
ESP_ERROR_CHECK(tg28_set_rail_voltage(tg28_rail_t::aldo1, 3300));
ESP_ERROR_CHECK(tg28_set_rail_enabled(tg28_rail_t::aldo1, true));
```

支持范围：

| 电源轨 | 电压范围 |
| --- | --- |
| DCDC1 | 1.5～3.4 V，100 mV/档 |
| DCDC2、DCDC3 | 0.5～1.2 V，10 mV/档；1.22～1.54 V，20 mV/档 |
| DCDC4 | 0.5～1.2 V，10 mV/档；1.22～1.84 V，20 mV/档 |
| ALDO1～4、BLDO1～2 | 0.5～3.5 V，100 mV/档 |
| CPUSLDO、DLDO2 | 0.5～1.4 V，50 mV/档 |
| DLDO1 | 0.5～3.4 V，100 mV/档 |

驱动还提供 DCDC 强制 PWM 和恒流模式控制。

## 充电与电源路径

```cpp
ESP_ERROR_CHECK(tg28_set_constant_charge_current(800));
ESP_ERROR_CHECK(tg28_set_charge_target_voltage(4200));
```

- 预充与终止电流：0～200 mA，25 mA/档；
- 恒流充电：0～200 mA，25 mA/档；300～1500 mA，100 mA/档；
- 满充电压：4.00、4.10、4.20、4.35 或 4.40 V；
- 输入限流：100、500、900、1000、1500 或 2000 mA；
- 输入限压：3.88～5.08 V，80 mV/档；
- 最低 VSYS：3.2～3.9 V，100 mV/档；关机电压：2.6～3.3 V，100 mV/档。

`tg28_set_charge_led_mode()` 可选择由充电状态自动控制，或手动关闭、常亮及 1 Hz / 4 Hz 闪烁。

充电设置必须匹配电芯规格、NTC、电源能力和系统散热。TG28-ESP 的 300 mA / 4.2 V 只是出厂默认值。

## ADC、IRQ 与看门狗

```cpp
ESP_ERROR_CHECK(tg28_set_adc_channels(
    TG28_ADC_BATTERY | TG28_ADC_VBUS | TG28_ADC_VSYS, true));

ESP_ERROR_CHECK(tg28_set_irq_enabled(
    tg28_irq::kVbusInsert | tg28_irq::kVbusRemove |
    tg28_irq::kChargeStart | tg28_irq::kChargeDone, true));
```

IRQ 状态为 RW1C。处理完成后使用 `tg28_clear_irq_status(mask)` 清除对应标志。看门狗支持 1、2、4、8、16、32、64、128 秒，启用后应用程序必须持续调用 `tg28_feed_watchdog()`。

## 电量计参数

`tg28_write_battery_profile()` 只接受与具体电芯匹配的 128 字节参数，写入后可自动逐字节回读校验。仓库不提供通用 profile，也不会在启动时自动写入。

```cpp
uint8_t profile[128] = {/* 已验证电芯参数 */};
ESP_ERROR_CHECK(tg28_write_battery_profile(profile, sizeof(profile), true));
```

不同电芯不能混用 profile。首次导入时应断开下游负载，并保留写入、回读和整轮充放电学习记录。
