# TG28-ESP PMIC · ESP-IDF Driver and Bring-Up Example

[中文](README.md) | **English**

This ESP-IDF driver targets TG28 and TG28-ESP-MOD. Common functions—including power rails, charging, fuel gauging, ADC, IRQ, and watchdog control—remain available through independent APIs. The default example is read-only and does not modify any TG28 register at startup.

The module design, full-function test board, four-layer reference layout, documentation package, and purchase or customization options are available on the [OSHWHub project page](https://oshwhub.com/mondraker/axp2101_2023-11-18_20-15-19).

## TG28-ESP Factory Defaults

| Item | Factory default |
| --- | --- |
| DCDC1 | 3.3 V, enabled at power-up |
| DCDC4 | 1.8 V, enabled at power-up |
| RTC-LDO1 | 3.0 V, always on |
| Other configurable rails | Disabled at power-up |
| Charge target voltage | 4.2 V |
| Constant-current charge current | 300 mA |
| Charging architecture | Single-cell Li-ion, NVDC switching charger |

These values apply to the fixed TG28-ESP factory configuration. Other customized TG28 batches may use different voltages and power-up sequences. The top marking alone is not sufficient to identify a device configuration.

## First Bring-Up

1. Disconnect the battery and every downstream load. Check assembly, shorts, battery polarity, and the I²C pull-ups.
2. Apply VBUS from a current-limited supply, then measure DCDC1, DCDC4, and RTC-LDO1 with a multimeter.
3. Flash the default firmware and use its read-only flow to check the chip ID, rail states, charging settings, and system status.
4. Connect loads one rail at a time only after the register readings agree with the measured voltages.
5. Connect the battery last. Before doing so, confirm its full-charge voltage, permitted charge current, NTC characteristics, and protection-board specifications.

> Do not connect downstream loads or call any write API before the output voltages have been verified. A mismatched configuration can damage the PMIC or downstream devices.

## Features

- DCDC1–4, ALDO1–4, BLDO1–2, CPUSLDO, and DLDO1–2: enable control, voltage setting, and readback;
- Charging and power path: pre-charge, constant-current and constant-voltage charging, termination current, input voltage/current limits, VSYS, and low-battery thresholds;
- Fuel gauge: state-of-charge readout, 128-byte cell-profile programming, and byte-by-byte verification;
- Status and measurements: VBUS, battery and charge-stage status, plus VBAT, VBUS, VSYS, TS, and die-temperature ADC readings;
- IRQ: 24-bit enable mask, status readback, and RW1C clearing;
- Watchdog, backup-battery charging, TS charge-protection control, and a four-byte data buffer.

See the [API documentation](docs/API.md) for the complete interface and supported settings.

## Build

ESP-IDF 5.4 or a compatible release is required:

```bash
idf.py set-target esp32
idf.py menuconfig
idf.py build
idf.py -p PORT flash monitor
```

The default SDA/SCL pins are GPIO21/GPIO22 on ESP32 and GPIO8/GPIO9 on ESP32-C3. They can be changed under the `TG28-ESP PMIC` menu.

By default, the serial console reports the chip ID, 13 configurable rails, system status, charging settings, input limits, ADC enable state, and fuel-gauge readings. RTC-LDO1 is not controlled by REG90/REG91; verify its 3.0 V output with a multimeter.

## Read-Only by Default and Controlled Writes

`main/main.cpp` calls read APIs only by default. Write APIs in the driver do not run automatically, and the repository does not include a battery-cell profile.

If the TG28-ESP runtime configuration must be restored, first disconnect the battery and every downstream load. Then enable both options in `menuconfig`, in this order:

1. `I have disconnected the battery and every downstream load`;
2. `Write TG28-ESP rail and charger settings at startup`.

This mode sets DCDC1 to 3.3 V and DCDC4 to 1.8 V, disables the other configurable rails, and writes the selected charge current and full-charge voltage. After the operation, disable both options and rebuild the read-only firmware.

These APIs change runtime registers only; they do not alter the device's factory eFuse configuration. Production firmware should explicitly configure only the functions required by its own power-up sequence, after the hardware configuration has been confirmed.

## Repository Layout

```text
main/
├─ main.cpp             Read-only bring-up flow by default
├─ tg28_i2c.*           ESP-IDF I²C transport
├─ tg28_registers.h     TG28 register addresses
└─ tg28_pmic.*          Complete driver API and encoding/decoding
docs/
└─ API.md               API groups, supported settings, and examples
```

## Validation Scope

- Register addresses, voltage ranges, and encodings were checked against the current TG28 switching-charger documentation;
- ESP32 and ESP32-C3 targets are built by GitHub Actions, with an additional local check using ESP-IDF 5.4.1;
- No additional hardware test was performed after this refactor. Measured voltage, ripple, temperature rise, and load capability depend on the actual board implementation.

## License

GPL-3.0. Third-party documentation and tools retain their respective copyright and license terms.
