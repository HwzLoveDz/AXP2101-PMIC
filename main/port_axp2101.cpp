#include <stdio.h>
#include <cstring>
// #include "sdkconfig.h"
#include "../build/config/sdkconfig.h"
#include "esp_log.h"
#include "esp_err.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef CONFIG_XPOWERS_CHIP_AXP2101

#define XPOWERS_CHIP_AXP2101
#include "XPowersLib.h"
static const char *TAG = "AXP2101";

static XPowersPMU PMU;

extern int pmu_register_read(uint8_t devAddr, uint8_t regAddr, uint8_t *data, uint8_t len);
extern int pmu_register_write_byte(uint8_t devAddr, uint8_t regAddr, uint8_t *data, uint8_t len);


uint8_t battary_param[128] = {
    0x01,0xf5,0x40,0x00,0x1b,0x1e,0x28,0x0f,0x0c,0x1e,0x32,0x02,0x14,0x05,0x0a,0x04,
    0x74,0xfb,0xfa,0x0d,0x43,0x00,0x80,0xfb,0x55,0x01,0xea,0x05,0x3d,0xf6,0x80,0xf6,
    0x76,0xfb,0x6c,0x00,0x5d,0x00,0x58,0xfb,0x4e,0x00,0x3a,0x00,0x3a,0xf6,0x30,0xf6,
    0x26,0xfb,0x1c,0x00,0x0d,0x00,0x08,0xfa,0xfe,0xff,0xea,0xff,0xea,0xf5,0xe0,0xf5,
    0xd6,0xfa,0xcc,0xff,0xbd,0xff,0xb8,0xfa,0xae,0x00,0xf6,0x00,0x00,0xf6,0x00,0xf6,
    0xc5,0x98,0x7e,0x66,0x4e,0x44,0x38,0x1a,0x12,0x0a,0xf6,0x00,0x00,0xf6,0x00,0xf6,
    0x00,0xfb,0x00,0x00,0xfb,0x00,0x00,0xfb,0x00,0x00,0xf6,0x00,0x00,0xf6,0x00,0xf6,
    0x00,0xfb,0x00,0x00,0xfb,0x00,0x00,0xfb,0x00,0x00,0xf6,0x00,0x00,0xf6,0x00,0xf6,
};


void pmu_w_battary_param(uint8_t (*param)[128])
{
    // 在PMU驱动初始化的时候，加载电池参数，对电量计进行配置
    // 具体步骤如下:

    // 1. Reset MCU: 寄存器reg17Hbit[2]先写 1 再写 0，即reg17H=0x04，reg17H=0x00
    PMU.setRegisterBit(XPOWERS_AXP2101_RESET_FUEL_GAUGE, 2); // reg17H=0x04
    vTaskDelay(10 / portTICK_PERIOD_MS);
    PMU.clrRegisterBit(XPOWERS_AXP2101_RESET_FUEL_GAUGE, 2); // reg17H=0x00
    vTaskDelay(10 / portTICK_PERIOD_MS);

    // 2. 使能BROM: 寄存器regA2Hbit[0]先写0再写1，即regA2H=0x00，regA2H=0x01
    // fuelGaugeControl(writeROM_bit4,enable_bit0)
    PMU.fuelGaugeControl(0, 0);   // regA2H=0x00
    vTaskDelay(10 / portTICK_PERIOD_MS);
    PMU.fuelGaugeControl(0, 1);   // regA2H=0x01
    vTaskDelay(10 / portTICK_PERIOD_MS);

    // 3. 向BROM寄存器依次写入电池参数：寄存器regA1H依次连续写入128个电池参数（即写128次regA1H，
    // 参数顺序不能乱）
    // 写入参数
    if ((uint8_t)(sizeof((*param)) / sizeof((*param)[0])) == 128){
        ESP_LOGI(TAG, "Battery parameters format correc!\n");
        ESP_LOGW(TAG,"Write battery parameters:");
        for (int i = 0; i < 128; i++) { 
            PMU.writeRegister(XPOWERS_AXP2101_BAT_PARAME, (*param)[i]);
            printf("0x%02X ", (*param)[i]);
            if ((i + 1) % 16 == 0) {
                printf("\n");  // 每 16 字节换行
            }
        }
    } else {
        ESP_LOGE(TAG, "Battery parameters format error!");
    }
    
    // 4. 重新使能BROM: 寄存器regA2Hbit[0]先写0再写1，即regA2H=0x00，regA2H=0x01
    PMU.fuelGaugeControl(0, 0);   //regA2H
    vTaskDelay(10 / portTICK_PERIOD_MS);
    PMU.fuelGaugeControl(0, 1);   //regA2H
    vTaskDelay(10 / portTICK_PERIOD_MS);

    // 5. 将BROM的数值读出比较：寄存器regA1H连续读出128个数值（即读128次regA1H），验证电池
    // 参数是否正确加载
    uint8_t brom_param[128] = {};
    ESP_LOGW(TAG,"Read battery parameters:");
    for (int i = 0; i < 128; i++) {
        brom_param[i] = PMU.getBatteryParameter();
        printf("0x%02X ", brom_param[i]);
        if ((i + 1) % 16 == 0) {
            printf("\n");
        }
    }

    // 6. 关闭BROM使能: 寄存器regA2Hbit[0]写0，即regA2H=0x00
    PMU.fuelGaugeControl(0, 0);   // regA2H=0x00
    vTaskDelay(10 / portTICK_PERIOD_MS);

    // 7. 选择数据接口：寄存器regA2Hbit[4]写1，即regA2H=0x10
    PMU.fuelGaugeControl(1, 0);   // regA2H=0x10
    vTaskDelay(10 / portTICK_PERIOD_MS);

    // 8. Reset MCU: 寄存器reg17Hbit[2]先写 1 再写 0，即reg17H=0x04，reg17H=0x00
    PMU.setRegisterBit(XPOWERS_AXP2101_RESET_FUEL_GAUGE, 2); // reg17H=0x04
    vTaskDelay(10 / portTICK_PERIOD_MS);
    PMU.clrRegisterBit(XPOWERS_AXP2101_RESET_FUEL_GAUGE, 2); // reg17H=0x00
    vTaskDelay(10 / portTICK_PERIOD_MS);

    // 电量计完成配置后，可通过寄存器REGA4H获取电量信息
}


void pmu_battary_info(){
        // 输出电池信息
    ESP_LOGW(TAG,"Battary info:");
    ESP_LOGW(TAG,"---------------------------------------------------------------------------------------------------------");

    ESP_LOGI(TAG, "Battary Voltage:%u mV", PMU.getBattVoltage());

    // The battery percentage may be inaccurate at first use, the PMU will automatically
    // learn the battery curve and will automatically calibrate the battery percentage
    // after a charge and discharge cycle
    ESP_LOGI(TAG, "battery percentage:%d %%", PMU.getBatteryPercent());

    uint8_t charge_status = PMU.getChargerStatus();
    if (charge_status == XPOWERS_AXP2101_CHG_TRI_STATE) {
        ESP_LOGI(TAG, "Charging procedure:tri_charge");
    } else if (charge_status == XPOWERS_AXP2101_CHG_PRE_STATE) {
        ESP_LOGI(TAG, "Charging procedure:pre_charge");
    } else if (charge_status == XPOWERS_AXP2101_CHG_CC_STATE) {
        ESP_LOGI(TAG, "Charging procedure:constant charge(CC)");
    } else if (charge_status == XPOWERS_AXP2101_CHG_CV_STATE) {
        ESP_LOGI(TAG, "Charging procedure:constant voltage(CV)");
    } else if (charge_status == XPOWERS_AXP2101_CHG_DONE_STATE) {
        ESP_LOGI(TAG, "Charging procedure:charge done");
    } else if (charge_status == XPOWERS_AXP2101_CHG_STOP_STATE) {
        ESP_LOGI(TAG, "Charging procedure:not charge");
    }
    ESP_LOGW(TAG,"---------------------------------------------------------------------------------------------------------");
    printf("\n");
}


void pmu_system_info(){
    // 输出电源系统状态信息
    ESP_LOGW(TAG,"Power status info:");
    ESP_LOGW(TAG,"---------------------------------------------------------------------------------------------------------");
    // ESP_LOGI(TAG,"CHARG   DISC   STBY    VBUSIN    VGOOD       VBUS       VSYS       CORE_TEMP");
    // ESP_LOGI(TAG,"(bool) (bool) (bool)   (bool)    (bool)       (mV)       (mV)       (*C)");
    ESP_LOGI(TAG, "\nCHARG      (%s)\nDISCHARG   (%s)\nSTANDBY    (%s)\nVBUSIN     (%s)\nVBUSGOOD   (%s)\nVBUS       (%d mV)\nVSYS       (%d mV)\nCORE_TEMP  (%.2f *C)",
             PMU.isCharging()? "YES" : "NO",
             PMU.isDischarge()? "YES" : "NO",
             PMU.isStandby()? "YES" : "NO",
             PMU.isVbusIn()? "YES" : "NO",
             PMU.isVbusGood()? "YES" : "NO",
             PMU.getVbusVoltage(),
             PMU.getSystemVoltage(),
             PMU.getTemperature());
    ESP_LOGW(TAG,"---------------------------------------------------------------------------------------------------------");
    printf("\n");
}


esp_err_t pmu_init()
{
    if (PMU.begin(AXP2101_SLAVE_ADDRESS, pmu_register_read, pmu_register_write_byte)) {
        ESP_LOGI(TAG, "Init PMU SUCCESS!");
    } else {
        ESP_LOGE(TAG, "Init PMU FAILED!");
        return ESP_FAIL;
    }

    // upload battary param
    pmu_w_battary_param(&battary_param);

    PMU.disableALDO1();
    PMU.disableALDO2();
    PMU.disableALDO3();
    PMU.disableALDO4();

    // Acce
    PMU.setALDO1Voltage(3300);
    PMU.enableALDO1();

    // Codec
    PMU.setALDO2Voltage(3300);
    PMU.enableALDO2();

    // Screen
    PMU.setALDO4Voltage(3300);
    PMU.enableALDO4();


/* use default setting
    //Turn off not use power channel
    PMU.disableDC2(); 
    PMU.disableDC3();
    PMU.disableDC4();
    PMU.disableDC5();

    PMU.disableALDO1();
    PMU.disableALDO2();
    PMU.disableALDO3();
    PMU.disableALDO4();
    PMU.disableBLDO1();
    PMU.disableBLDO2();

    PMU.disableCPUSLDO();
    PMU.disableDLDO1();
    PMU.disableDLDO2();


    //ESP32s3 Core VDD
    PMU.setDC3Voltage(3300);
    PMU.enableDC3();

    //Extern 3.3V VDD
    PMU.setDC1Voltage(3300);
    PMU.enableDC1();

    // CAM DVDD  1500~1800
    PMU.setALDO1Voltage(1800);
    // PMU.setALDO1Voltage(1500);
    PMU.enableALDO1();

    // CAM DVDD 2500~2800
    PMU.setALDO2Voltage(2800);
    PMU.enableALDO2();

    // CAM AVDD 2800~3000
    PMU.setALDO4Voltage(3000);
    PMU.enableALDO4();

    // PIR VDD 3300
    PMU.setALDO3Voltage(3300);
    PMU.enableALDO3();

    // OLED VDD 3300
    PMU.setBLDO1Voltage(3300);
    PMU.enableBLDO1();

    // MIC VDD 33000
    PMU.setBLDO2Voltage(3300);
    PMU.enableBLDO2();
*/


    // 输出各路电源信息
    printf("\n");
    ESP_LOGW(TAG, "Power channel info:");
    ESP_LOGW(TAG,"---------------------------------------------------------------------------------------------------------");
    ESP_LOGI(TAG, "DCDC===========================");
    ESP_LOGI(TAG, "DC1  : %s   Voltage:%u mV ",  PMU.isEnableDC1()  ? "o" : "-", PMU.getDC1Voltage());
    ESP_LOGI(TAG, "DC2  : %s   Voltage:%u mV ",  PMU.isEnableDC2()  ? "o" : "-", PMU.getDC2Voltage());
    ESP_LOGI(TAG, "DC3  : %s   Voltage:%u mV ",  PMU.isEnableDC3()  ? "o" : "-", PMU.getDC3Voltage());
    ESP_LOGI(TAG, "DC4  : %s   Voltage:%u mV ",  PMU.isEnableDC4()  ? "o" : "-", PMU.getDC4Voltage());
    ESP_LOGI(TAG, "DC5  : %s   Voltage:%u mV ",  PMU.isEnableDC5()  ? "o" : "-", PMU.getDC5Voltage());
    ESP_LOGI(TAG, "ALDO===========================");
    ESP_LOGI(TAG, "ALDO1: %s   Voltage:%u mV",  PMU.isEnableALDO1()  ? "o" : "-", PMU.getALDO1Voltage());
    ESP_LOGI(TAG, "ALDO2: %s   Voltage:%u mV",  PMU.isEnableALDO2()  ? "o" : "-", PMU.getALDO2Voltage());
    ESP_LOGI(TAG, "ALDO3: %s   Voltage:%u mV",  PMU.isEnableALDO3()  ? "o" : "-", PMU.getALDO3Voltage());
    ESP_LOGI(TAG, "ALDO4: %s   Voltage:%u mV",  PMU.isEnableALDO4()  ? "o" : "-", PMU.getALDO4Voltage());
    ESP_LOGI(TAG, "BLDO===========================");
    ESP_LOGI(TAG, "BLDO1: %s   Voltage:%u mV",  PMU.isEnableBLDO1()  ? "o" : "-", PMU.getBLDO1Voltage());
    ESP_LOGI(TAG, "BLDO2: %s   Voltage:%u mV",  PMU.isEnableBLDO2()  ? "o" : "-", PMU.getBLDO2Voltage());
    ESP_LOGI(TAG, "CPUSLDO========================");
    ESP_LOGI(TAG, "CPUSLDO: %s Voltage:%u mV",  PMU.isEnableCPUSLDO() ? "o" : "-", PMU.getCPUSLDOVoltage());
    ESP_LOGI(TAG, "DLDO===========================");
    ESP_LOGI(TAG, "DLDO1: %s   Voltage:%u mV",  PMU.isEnableDLDO1()  ? "o" : "-", PMU.getDLDO1Voltage());
    ESP_LOGI(TAG, "DLDO2: %s   Voltage:%u mV",  PMU.isEnableDLDO2()  ? "o" : "-", PMU.getDLDO2Voltage());
    ESP_LOGI(TAG, "===============================");
    ESP_LOGW(TAG,"---------------------------------------------------------------------------------------------------------");
    printf("\n");

    PMU.clearIrqStatus();

    PMU.enableTemperatureMeasure();
    // Enable internal ADC detection
    PMU.enableBattDetection();
    PMU.enableVbusVoltageMeasure();
    PMU.enableBattVoltageMeasure();
    PMU.enableSystemVoltageMeasure();
    // PMU.enableGauge();
    // PMU.enableCellbatteryCharge();

    // It is necessary to disable the detection function of the TS pin on the board
    // without the battery temperature detection function, otherwise it will cause abnormal charging
    PMU.disableTSPinMeasure();

    // Disable all interrupts
    PMU.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
    // Clear all interrupt flags
    PMU.clearIrqStatus();
    // Enable the required interrupt function
    PMU.enableIRQ(
        XPOWERS_AXP2101_BAT_INSERT_IRQ    | XPOWERS_AXP2101_BAT_REMOVE_IRQ      |   //BATTERY
        XPOWERS_AXP2101_VBUS_INSERT_IRQ   | XPOWERS_AXP2101_VBUS_REMOVE_IRQ     |   //VBUS
        XPOWERS_AXP2101_PKEY_SHORT_IRQ    | XPOWERS_AXP2101_PKEY_LONG_IRQ       |   //POWER KEY
        XPOWERS_AXP2101_BAT_CHG_DONE_IRQ  | XPOWERS_AXP2101_BAT_CHG_START_IRQ       //CHARGE
        // XPOWERS_AXP2101_PKEY_NEGATIVE_IRQ | XPOWERS_AXP2101_PKEY_POSITIVE_IRQ   |   //POWER KEY
    );

    // below this value will turn off the power
    PMU.setVbusVoltageLimit(XPOWERS_AXP2101_VBUS_VOL_LIM_4V36);
    // Set the maximum current of the PMU VBUS input,
    // higher than this value will turn off the PMU
    PMU.setVbusCurrentLimit(XPOWERS_AXP2101_VBUS_CUR_LIM_2000MA);
    // Set VSY off voltage as 2600mV , Adjustment range 2600mV ~ 3300mV
    PMU.setSysPowerDownVoltage(2600);

    // Set the precharge charging current
    PMU.setPrechargeCurr(XPOWERS_AXP2101_PRECHARGE_50MA);
    // Set constant current charge current limit
    PMU.setChargerConstantCurr(XPOWERS_AXP2101_CHG_CUR_1000MA);
    // Set stop charging termination current
    PMU.setChargerTerminationCurr(XPOWERS_AXP2101_CHG_ITERM_25MA);
    // Set charge cut-off voltage
    PMU.setChargeTargetVoltage(XPOWERS_AXP2101_CHG_VOL_4V2);
    // CHG led mode set
    PMU.setChargingLedMode(XPOWERS_CHG_LED_CTRL_CHG);

    // Set button battery voltage to 3.3V
    PMU.setButtonBatteryChargeVoltage(3300);
    // Disable button battery charge function
    PMU.disableButtonBatteryCharge();

    // Set the power level to be lower than 15% and send an interrupt to the host`
    PMU.setLowBatWarnThreshold(10);
    // Set the power level to be lower than 5% and turn off the power supply
    PMU.setLowBatShutdownThreshold(5);

    // 输出电源系统状态信息
    ESP_LOGW(TAG,"Power status info:");
    ESP_LOGW(TAG,"---------------------------------------------------------------------------------------------------------");
    ESP_LOGI(TAG,"CHARG   DISC   STBY    VBUSIN    VGOOD       VBUS       VSYS       CORE_TEMP");
    ESP_LOGI(TAG,"(bool) (bool) (bool)   (bool)    (bool)       (mV)       (mV)       (*C)");
    ESP_LOGI(TAG, "(%s)  (%s)   (%s)     (%s)     (%s)  (%d mV)  (%d mV)  (%.3f *C)",
             PMU.isCharging()? "YES" : "NO",
             PMU.isDischarge()? "YES" : "NO",
             PMU.isStandby()? "YES" : "NO",
             PMU.isVbusIn()? "YES" : "NO",
             PMU.isVbusGood()? "YES" : "NO",
             PMU.getVbusVoltage(),
             PMU.getSystemVoltage(),
             PMU.getTemperature());
    ESP_LOGW(TAG,"---------------------------------------------------------------------------------------------------------");
    printf("\n");

    // 输出电池信息
    ESP_LOGW(TAG,"Battary info:");
    ESP_LOGW(TAG,"---------------------------------------------------------------------------------------------------------");
    const uint16_t currTable[] = {
        0, 0, 0, 0, 100, 125, 150, 175, 200, 300, 400, 500, 600, 700, 800, 900, 1000, 1100, 1200, 1300, 1400, 1500
    };
    uint8_t val = PMU.getChargerConstantCurr();
    ESP_LOGI(TAG, "Charge Constant Current:%u mA", currTable[val]);

    const uint16_t tableVoltage[] = {
        0, 4000, 4100, 4200, 4350, 4400, 255
    };
    val = PMU.getChargeTargetVoltage();
    ESP_LOGI(TAG, "Charge Target Voltage:%u mV", tableVoltage[val]);

    ESP_LOGI(TAG, "Battary Voltage:%u mV", PMU.getBattVoltage());

    // The battery percentage may be inaccurate at first use, the PMU will automatically
    // learn the battery curve and will automatically calibrate the battery percentage
    // after a charge and discharge cycle
    ESP_LOGI(TAG, "battery percentage:%d %%", PMU.getBatteryPercent());

    uint8_t charge_status = PMU.getChargerStatus();
    if (charge_status == XPOWERS_AXP2101_CHG_TRI_STATE) {
        ESP_LOGI(TAG, "Charging procedure:tri_charge");
    } else if (charge_status == XPOWERS_AXP2101_CHG_PRE_STATE) {
        ESP_LOGI(TAG, "Charging procedure:pre_charge");
    } else if (charge_status == XPOWERS_AXP2101_CHG_CC_STATE) {
        ESP_LOGI(TAG, "Charging procedure:constant charge(CC)");
    } else if (charge_status == XPOWERS_AXP2101_CHG_CV_STATE) {
        ESP_LOGI(TAG, "Charging procedure:constant voltage(CV)");
    } else if (charge_status == XPOWERS_AXP2101_CHG_DONE_STATE) {
        ESP_LOGI(TAG, "Charging procedure:charge done");
    } else if (charge_status == XPOWERS_AXP2101_CHG_STOP_STATE) {
        ESP_LOGI(TAG, "Charging procedure:not charge");
    }
    ESP_LOGW(TAG,"---------------------------------------------------------------------------------------------------------");
    printf("\n");

    // Set the watchdog trigger event type
    // PMU.setWatchdogConfig(XPOWERS_AXP2101_WDT_IRQ_TO_PIN);
    // Set watchdog timeout
    PMU.setWatchdogTimeout(XPOWERS_AXP2101_WDT_TIMEOUT_8S);
    // Enable watchdog to trigger interrupt event
    PMU.enableWatchdog();
    return ESP_OK;
}


void pmu_isr_handler()
{
// Get PMU Interrupt Status Register
    PMU.getIrqStatus();

    if (PMU.isDropWarningLevel2Irq()) {
        ESP_LOGI(TAG, "isDropWarningLevel2");
    }
    if (PMU.isDropWarningLevel1Irq()) {
        ESP_LOGI(TAG, ">>>> isDropWarningLevel1 <<<<");
    }
    if (PMU.isGaugeWdtTimeoutIrq()) {
        ESP_LOGI(TAG, "isWdtTimeout");
    }
    if (PMU.isBatChargerOverTemperatureIrq()) {
        ESP_LOGI(TAG, "isBatChargeOverTemperature");
    }
    if (PMU.isBatWorkOverTemperatureIrq()) {
        ESP_LOGI(TAG, "isBatWorkOverTemperature");
    }
    if (PMU.isBatWorkUnderTemperatureIrq()) {
        ESP_LOGI(TAG, "isBatWorkUnderTemperature");
    }
    if (PMU.isVbusInsertIrq()) {
        ESP_LOGI(TAG, "isVbusInsert");
    }
    if (PMU.isVbusRemoveIrq()) {
        ESP_LOGI(TAG, "isVbusRemove");
    }
    if (PMU.isBatInsertIrq()) {
        ESP_LOGI(TAG, "isBatInsert");
    }
    if (PMU.isBatRemoveIrq()) {
        ESP_LOGI(TAG, "isBatRemove");
    }
    if (PMU.isPekeyShortPressIrq()) {
        ESP_LOGI(TAG, "isPekeyShortPress");
    }
    if (PMU.isPekeyLongPressIrq()) {
        ESP_LOGI(TAG, "isPekeyLongPress");
    }
    if (PMU.isPekeyNegativeIrq()) {
        ESP_LOGI(TAG, "isPekeyNegative");
    }
    if (PMU.isPekeyPositiveIrq()) {
        ESP_LOGI(TAG, "isPekeyPositive");
    }
    if (PMU.isWdtExpireIrq()) {
        ESP_LOGI(TAG, "isWdtExpire");
        pmu_battary_info();
        pmu_system_info();
    }
    if (PMU.isLdoOverCurrentIrq()) {
        ESP_LOGI(TAG, "isLdoOverCurrentIrq");
    }
    if (PMU.isBatfetOverCurrentIrq()) {
        ESP_LOGI(TAG, "isBatfetOverCurrentIrq");
    }
    if (PMU.isBatChagerDoneIrq()) {
        ESP_LOGI(TAG, "isBatChagerDone");
    }
    if (PMU.isBatChagerStartIrq()) {
        ESP_LOGI(TAG, "isBatChagerStart");
    }
    if (PMU.isBatDieOverTemperatureIrq()) {
        ESP_LOGI(TAG, "isBatDieOverTemperature");
    }
    if (PMU.isChagerOverTimeoutIrq()) {
        ESP_LOGI(TAG, "isChagerOverTimeout");
    }
    if (PMU.isBatOverVoltageIrq()) {
        ESP_LOGI(TAG, "isBatOverVoltage");
    }
    // Clear PMU Interrupt Status Register
    PMU.clearIrqStatus();
}


/*
! WARN:
Please do not run the example without knowing the external load voltage of the PMU,
it may burn your external load, please check the voltage setting before running the example,
if there is any loss, please bear it by yourself
*/
/*
void pwr_channel_voltage_setting_example(){
    // DC1 IMAX=2A
    // 1500~3400mV,100mV/step,20steps
    vol = 1500;
    for (int i = 0; i < 20; ++i) {
        power.setDC1Voltage(vol);
        vol += 100;
        Serial.printf("DC1  :%s   Voltage:%u mV \n",  power.isEnableDC1()  ? "ENABLE" : "DISABLE", power.getDC1Voltage());
    }


    // DC2 IMAX=2A
    // 500~1200mV  10mV/step,71steps
    vol = 500;
    for (int i = 0; i < 71; ++i) {
        power.setDC2Voltage(vol);
        delay(1);
        targetVol = power.getDC2Voltage();
        Serial.printf("[%d]DC2  :%s   Voltage:%u mV \n", i,  power.isEnableDC2()  ? "ENABLE" : "DISABLE", targetVol );
        if (targetVol != vol)Serial.println(">>> FAILED!");
        vol += 10;
    }

    // DC2 IMAX=2A
    // 1220~1540mV 20mV/step,17steps
    vol = 1220;
    for (int i = 0; i < 17; ++i) {
        power.setDC2Voltage(vol);
        delay(1);
        targetVol = power.getDC2Voltage();
        Serial.printf("[%u]DC2  :%s   Voltage:%u mV \n", i,  power.isEnableDC2()  ? "ENABLE" : "DISABLE", targetVol );
        if (targetVol != vol)Serial.println(">>> FAILED!");
        vol += 20;
    }

    // DC3 IMAX = 2A
    // 500~1200mV,10mV/step,71steps
    vol = 500;
    for (int i = 0; i < 71; ++i) {
        power.setDC3Voltage(vol);
        delay(1);
        targetVol = power.getDC3Voltage();
        Serial.printf("[%u]DC3  :%s   Voltage:%u mV \n", i,  power.isEnableDC3()  ? "ENABLE" : "DISABLE", targetVol );
        if (targetVol != vol)Serial.println(">>> FAILED!");
        vol += 10;
    }

    // DC3 IMAX = 2A
    // 1220~1540mV,20mV/step,17steps
    vol = 1220;
    for (int i = 0; i < 17; ++i) {
        power.setDC3Voltage(vol);
        delay(1);
        targetVol = power.getDC3Voltage();
        Serial.printf("[%u]DC3  :%s   Voltage:%u mV \n", i,  power.isEnableDC3()  ? "ENABLE" : "DISABLE", targetVol );
        if (targetVol != vol)Serial.println(">>> FAILED!");
        vol += 20;
    }


    // DC3 IMAX = 2A
    // 1600~3400mV,100mV/step,19steps
    vol = 1600;
    for (int i = 0; i < 19; ++i) {
        power.setDC3Voltage(vol);
        delay(1);
        targetVol = power.getDC3Voltage();
        Serial.printf("[%u]DC3  :%s   Voltage:%u mV \n", i,  power.isEnableDC3()  ? "ENABLE" : "DISABLE", targetVol );
        if (targetVol != vol)Serial.println(">>> FAILED!");
        vol += 100;
    }


    // DCDC4 IMAX=1.5A
    // 500~1200mV,10mV/step,71steps
    vol = 500;
    for (int i = 0; i < 71; ++i) {
        power.setDC4Voltage(vol);
        delay(1);
        targetVol = power.getDC4Voltage();
        Serial.printf("[%u]DC4  :%s   Voltage:%u mV \n", i,  power.isEnableDC4()  ? "ENABLE" : "DISABLE", targetVol );
        if (targetVol != vol)Serial.println(">>> FAILED!");
        vol += 10;
    }

    // DCDC4 IMAX=1.5A
    // 1220~1840mV,20mV/step,32steps
    vol = 1220;
    for (int i = 0; i < 32; ++i) {
        power.setDC4Voltage(vol);
        delay(1);
        targetVol = power.getDC4Voltage();
        Serial.printf("[%u]DC4  :%s   Voltage:%u mV \n", i,  power.isEnableDC4()  ? "ENABLE" : "DISABLE", targetVol );
        if (targetVol != vol)Serial.println(">>> FAILED!");
        vol += 20;
    }

    // DC5 IMAX=2A
    // 1200mV
    power.setDC5Voltage(1200);
    targetVol = power.getDC5Voltage();
    Serial.printf("[0]DC5  :%s   Voltage:%u mV \n",   power.isEnableDC5()  ? "ENABLE" : "DISABLE", targetVol );


    // DC5 IMAX=2A
    // 1400~3700mV,100mV/step,24steps
    vol = 1400;
    for (int i = 0; i < 24; ++i) {
        power.setDC5Voltage(vol);
        delay(1);
        targetVol = power.getDC5Voltage();
        Serial.printf("[%u]DC5  :%s   Voltage:%u mV \n", i,  power.isEnableDC5()  ? "ENABLE" : "DISABLE", targetVol );
        if (targetVol != vol)Serial.println(">>> FAILED!");
        vol += 100;
    }



    //ALDO1 IMAX=300mA
    //500~3500mV, 100mV/step,31steps
    vol = 500;
    for (int i = 0; i < 31; ++i) {
        power.setALDO1Voltage(vol);
        delay(1);
        targetVol = power.getALDO1Voltage();
        Serial.printf("[%u]ALDO1  :%s   Voltage:%u mV \n", i,  power.isEnableALDO1()  ? "ENABLE" : "DISABLE", targetVol );
        if (targetVol != vol)Serial.println(">>> FAILED!");
        vol += 100;
    }

    //ALDO2 IMAX=300mA
    //500~3500mV, 100mV/step,31steps
    vol = 500;
    for (int i = 0; i < 31; ++i) {
        power.setALDO2Voltage(vol);
        delay(1);
        targetVol = power.getALDO2Voltage();
        Serial.printf("[%u]ALDO2  :%s   Voltage:%u mV \n", i,  power.isEnableALDO2()  ? "ENABLE" : "DISABLE", targetVol );
        if (targetVol != vol)Serial.println(">>> FAILED!");
        vol += 100;
    }

    //ALDO3 IMAX=300mA
    //500~3500mV, 100mV/step,31steps
    vol = 500;
    for (int i = 0; i < 31; ++i) {
        power.setALDO3Voltage(vol);
        delay(1);
        targetVol = power.getALDO3Voltage();
        Serial.printf("[%u]ALDO3  :%s   Voltage:%u mV \n", i,  power.isEnableALDO3()  ? "ENABLE" : "DISABLE", targetVol );
        if (targetVol != vol)Serial.println(">>> FAILED!");
        vol += 100;
    }

    //ALDO4 IMAX=300mA
    //500~3500mV, 100mV/step,31steps
    vol = 500;
    for (int i = 0; i < 31; ++i) {
        power.setALDO4Voltage(vol);
        delay(1);
        targetVol = power.getALDO4Voltage();
        Serial.printf("[%u]ALDO4  :%s   Voltage:%u mV \n", i,  power.isEnableALDO4()  ? "ENABLE" : "DISABLE", targetVol );
        if (targetVol != vol)Serial.println(">>> FAILED!");
        vol += 100;
    }

    //BLDO1 IMAX=300mA
    //500~3500mV, 100mV/step,31steps
    vol = 500;
    for (int i = 0; i < 31; ++i) {
        power.setBLDO1Voltage(vol);
        delay(1);
        targetVol = power.getBLDO1Voltage();
        Serial.printf("[%u]BLDO1  :%s   Voltage:%u mV \n", i,  power.isEnableBLDO1()  ? "ENABLE" : "DISABLE", targetVol );
        if (targetVol != vol)Serial.println(">>> FAILED!");
        vol += 100;
    }

    //BLDO2 IMAX=300mA
    //500~3500mV, 100mV/step,31steps
    vol = 500;
    for (int i = 0; i < 31; ++i) {
        power.setBLDO2Voltage(vol);
        delay(1);
        targetVol = power.getBLDO2Voltage();
        Serial.printf("[%u]BLDO2  :%s   Voltage:%u mV \n", i,  power.isEnableBLDO2()  ? "ENABLE" : "DISABLE", targetVol );
        if (targetVol != vol)Serial.println(">>> FAILED!");
        vol += 100;
    }


    //CPUSLDO IMAX=30mA
    //500~1400mV,50mV/step,19steps
    vol = 500;
    for (int i = 0; i < 19; ++i) {
        power.setCPUSLDOVoltage(vol);
        delay(1);
        targetVol = power.getCPUSLDOVoltage();
        Serial.printf("[%u]CPUSLDO  :%s   Voltage:%u mV \n", i,  power.isEnableCPUSLDO()  ? "ENABLE" : "DISABLE", targetVol );
        if (targetVol != vol)Serial.println(">>> FAILED!");
        vol += 50;
    }

    //DLDO1 IMAX=300mA
    //500~3400mV, 100mV/step,29steps
    vol = 500;
    for (int i = 0; i < 29; ++i) {
        power.setDLDO1Voltage(vol);
        delay(1);
        targetVol = power.getDLDO1Voltage();
        Serial.printf("[%u]DLDO1  :%s   Voltage:%u mV \n", i,  power.isEnableDLDO1()  ? "ENABLE" : "DISABLE", targetVol );
        if (targetVol != vol)Serial.println(">>> FAILED!");
        vol += 100;
    }

    //DLDO2 IMAX=300mA
    //500~1400mV, 50mV/step,2steps
    vol = 500;
    for (int i = 0; i < 29; ++i) {
        power.setDLDO2Voltage(vol);
        delay(1);
        targetVol = power.getDLDO2Voltage();
        Serial.printf("[%u]DLDO2  :%s   Voltage:%u mV \n", i,  power.isEnableDLDO2()  ? "ENABLE" : "DISABLE", targetVol );
        if (targetVol != vol)Serial.println(">>> FAILED!");
        vol += 100;
    }
}
*/

#endif  /*CONFIG_XPOWERS_AXP2101_CHIP_AXP2102*/

