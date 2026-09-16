# TG28-ESP PMIC · ESP-IDF 驱动与联调示例

这是面向 TG28 / TG28-ESP-MOD 的 ESP-IDF 驱动。电源轨、充电、电量计、ADC、IRQ 与看门狗等常用功能均保留为独立 API；默认示例只读取状态，不会在启动时修改任何 TG28 寄存器。

## TG28-ESP 默认配置

| 项目 | 默认值 |
| --- | --- |
| DCDC1 | 3.3 V，上电开启 |
| DCDC4 | 1.8 V，上电开启 |
| RTC-LDO1 | 3.0 V，常开 |
| 其余可配置电源轨 | 上电关闭 |
| 充电目标电压 | 4.2 V |
| 恒流充电电流 | 300 mA |
| 充电架构 | 单节锂电池、NVDC 开关充电 |

这些是 TG28-ESP 的固定出厂配置。其他 TG28 定制批次可能采用不同电压和上电时序，仅凭芯片顶层丝印无法确认配置。

## 首次联调

1. 断开电池和所有下游负载，检查焊接、短路、电池极性和 I²C 上拉。
2. 使用限流电源接入 VBUS，用万用表确认 DCDC1、DCDC4 与 RTC-LDO1 的实测电压。
3. 烧录默认固件，只读核对芯片 ID、电源轨状态、充电设置和系统状态。
4. 回读值与实测电压一致后，再按实际需求逐路连接负载。
5. 电池最后接入；接入前核对满充电压、允许充电电流、NTC 与保护板规格。

> 未确认电压前不要连接下游负载，也不要调用任何写接口。配置不匹配可能损坏 PMIC 或下游器件。

## 功能

- DCDC1～4、ALDO1～4、BLDO1～2、CPUSLDO、DLDO1～2：开关、设压与回读；
- 充电与电源路径：预充、恒流、恒压、终止电流、输入限压/限流、VSYS 与低电量阈值；
- 电量计：SOC 读取、128 字节电芯参数写入及逐字节校验；
- 状态与采样：VBUS、电池、充电阶段、VBAT、VBUS、VSYS、TS 与芯片温度；
- IRQ：24 位使能、状态读取和 RW1C 清除；
- 看门狗、备用电池充电、TS 充电保护控制和 4 字节数据缓冲区。

详细接口和受支持档位见 [API 文档](docs/API.md)。

## 编译

需要 ESP-IDF 5.4 或兼容版本：

```bash
idf.py set-target esp32
idf.py menuconfig
idf.py build
idf.py -p PORT flash monitor
```

ESP32 默认 SDA/SCL 为 GPIO21/GPIO22；ESP32-C3 默认是 GPIO8/GPIO9，可在 `TG28-ESP PMIC` 菜单中修改。

默认串口会输出芯片 ID、13 路可配置电源、系统状态、充电设置、输入限值、ADC 使能状态和电量计读数。RTC-LDO1 不由 REG90/REG91 控制，3.0 V 仍需用万用表确认。

## 默认只读与受控写入

`main/main.cpp` 默认只调用读取接口。驱动中的写接口不会自行执行，项目也不内置任何电芯 profile。

确需恢复 TG28-ESP 运行配置时，先断开电池和全部下游负载，再在 `menuconfig` 中依次开启：

1. `I have disconnected the battery and every downstream load`；
2. `Write TG28-ESP rail and charger settings at startup`。

该模式会设置 DCDC1 3.3 V、DCDC4 1.8 V，关闭其余可配置电源轨，并写入所选充电电流和满充电压。操作完成后关闭这两个选项，重新编译为只读固件。

这些 API 修改的是当前运行寄存器，不会改写芯片的 eFuse 出厂配置。量产程序应在硬件配置确认后，按自身电源时序显式调用所需接口。

## 目录

```text
main/
├─ main.cpp             默认只读联调流程
├─ tg28_i2c.*           ESP-IDF I²C 传输层
├─ tg28_registers.h     TG28 寄存器地址
└─ tg28_pmic.*          完整驱动接口与编解码
docs/
└─ API.md               接口分类、档位和示例
```

## 验证范围

- 已按当前 TG28 开关充版本资料核对寄存器地址、电压范围和编码；
- ESP32 与 ESP32-C3 目标由 GitHub Actions 编译，本地使用 ESP-IDF 5.4.1 复核；
- 本次重构未重复进行实机测试，实测电压、纹波、温升与带载能力以具体板卡为准。

## License

GPL-3.0。第三方资料和工具保留各自版权与许可。
