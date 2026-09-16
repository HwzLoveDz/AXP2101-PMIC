#pragma once

#include <cstddef>
#include <cstdint>

namespace tg28_reg {

constexpr uint8_t kStatus1 = 0x00;
constexpr uint8_t kStatus2 = 0x01;
constexpr uint8_t kChipId = 0x03;
constexpr uint8_t kDataBuffer0 = 0x04;
constexpr uint8_t kCommonConfig = 0x10;
constexpr uint8_t kBatfetControl = 0x12;
constexpr uint8_t kDieTemperatureControl = 0x13;
constexpr uint8_t kMinimumSystemVoltage = 0x14;
constexpr uint8_t kInputVoltageLimit = 0x15;
constexpr uint8_t kInputCurrentLimit = 0x16;
constexpr uint8_t kFuelGaugeReset = 0x17;
constexpr uint8_t kChargeGaugeWatchdogControl = 0x18;
constexpr uint8_t kWatchdogControl = 0x19;
constexpr uint8_t kLowBatteryWarning = 0x1A;
constexpr uint8_t kPowerOnStatus = 0x20;
constexpr uint8_t kPowerOffStatus = 0x21;
constexpr uint8_t kPowerOffEnable = 0x22;
constexpr uint8_t kPowerOffVoltage = 0x24;

constexpr uint8_t kAdcChannelEnable = 0x30;
constexpr uint8_t kBatteryVoltageHigh = 0x34;
constexpr uint8_t kTsVoltageHigh = 0x36;
constexpr uint8_t kVbusVoltageHigh = 0x38;
constexpr uint8_t kSystemVoltageHigh = 0x3A;
constexpr uint8_t kDieTemperatureHigh = 0x3C;

constexpr uint8_t kIrqEnable1 = 0x40;
constexpr uint8_t kIrqEnable2 = 0x41;
constexpr uint8_t kIrqEnable3 = 0x42;
constexpr uint8_t kIrqStatus1 = 0x48;
constexpr uint8_t kIrqStatus2 = 0x49;
constexpr uint8_t kIrqStatus3 = 0x4A;

constexpr uint8_t kTsPinControl = 0x50;
constexpr uint8_t kPrechargeCurrent = 0x61;
constexpr uint8_t kConstantChargeCurrent = 0x62;
constexpr uint8_t kTerminationCurrent = 0x63;
constexpr uint8_t kChargeTargetVoltage = 0x64;
constexpr uint8_t kThermalRegulation = 0x65;
constexpr uint8_t kChargeSafetyTimer = 0x67;
constexpr uint8_t kBatteryDetection = 0x68;
constexpr uint8_t kChargeLedControl = 0x69;
constexpr uint8_t kBackupBatteryVoltage = 0x6A;

constexpr uint8_t kDcdcEnable = 0x80;
constexpr uint8_t kDcdcMode = 0x81;
constexpr uint8_t kDcdc1Voltage = 0x82;
constexpr uint8_t kDcdc2Voltage = 0x83;
constexpr uint8_t kDcdc3Voltage = 0x84;
constexpr uint8_t kDcdc4Voltage = 0x85;
constexpr uint8_t kLdoEnable1 = 0x90;
constexpr uint8_t kLdoEnable2 = 0x91;
constexpr uint8_t kAldo1Voltage = 0x92;
constexpr uint8_t kAldo2Voltage = 0x93;
constexpr uint8_t kAldo3Voltage = 0x94;
constexpr uint8_t kAldo4Voltage = 0x95;
constexpr uint8_t kBldo1Voltage = 0x96;
constexpr uint8_t kBldo2Voltage = 0x97;
constexpr uint8_t kCpuSldoVoltage = 0x98;
constexpr uint8_t kDldo1Voltage = 0x99;
constexpr uint8_t kDldo2Voltage = 0x9A;

constexpr uint8_t kBatteryParameter = 0xA1;
constexpr uint8_t kFuelGaugeControl = 0xA2;
constexpr uint8_t kBatteryPercent = 0xA4;

constexpr uint8_t kExpectedChipId = 0x4A;
constexpr size_t kBatteryProfileSize = 128;

}  // namespace tg28_reg
