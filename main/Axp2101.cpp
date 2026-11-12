#include "Axp2101.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "axp2101";

static const uint8_t s_address = 0x34;
// Short so that a stuck bus cannot keep the watch awake.
static const int s_timeoutMs = 50;

static const uint8_t REG_STATUS1 = 0x00;
static const uint8_t REG_STATUS2 = 0x01;
static const uint8_t REG_COMMON_CONFIG = 0x10;
static const uint8_t REG_MODULE_ENABLE = 0x18;
static const uint8_t REG_PWRON_STATUS = 0x20;
static const uint8_t REG_PWROFF_STATUS = 0x21;
static const uint8_t REG_SLEEP_WAKEUP = 0x26;
static const uint8_t REG_KEY_TIMING = 0x27;
static const uint8_t REG_ADC_ENABLE = 0x30;
static const uint8_t REG_TS_CONTROL = 0x50;
static const uint8_t REG_VBAT_HIGH = 0x34;
static const uint8_t REG_IRQ_ENABLE0 = 0x40;
static const uint8_t REG_IRQ_ENABLE1 = 0x41;
static const uint8_t REG_IRQ_ENABLE2 = 0x42;
static const uint8_t REG_IRQ_STATUS0 = 0x48;
static const uint8_t REG_IRQ_STATUS1 = 0x49;
static const uint8_t REG_IRQ_STATUS2 = 0x4A;
static const uint8_t REG_PRECHARGE_CURRENT = 0x61;
static const uint8_t REG_CHARGE_CURRENT = 0x62;
static const uint8_t REG_TERMINATION_CURRENT = 0x63;
static const uint8_t REG_CHARGE_VOLTAGE = 0x64;
static const uint8_t REG_BACKUP_CHARGE_VOLTAGE = 0x6A;
static const uint8_t REG_DCDC_ENABLE = 0x80;
static const uint8_t REG_DCDC_PWM = 0x81;
static const uint8_t REG_DCDC1_VOLTAGE = 0x82;
static const uint8_t REG_LDO_ENABLE0 = 0x90;
static const uint8_t REG_LDO_ENABLE1 = 0x91;
static const uint8_t REG_ALDO1_VOLTAGE = 0x92;
static const uint8_t REG_BATTERY_PERCENT = 0xA4;

static const uint8_t IRQ1_KEY_SHORT = 1 << 3;
static const uint8_t IRQ1_KEY_LONG = 1 << 2;

esp_err_t Axp2101::init( i2c_master_bus_handle_t bus )
{
    i2c_device_config_t config = {};
    config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    config.device_address = s_address;
    config.scl_speed_hz = 400000;
    return i2c_master_bus_add_device( bus, &config, &m_device );
}

esp_err_t Axp2101::readRegister( uint8_t reg, uint8_t &value )
{
    return i2c_master_transmit_receive( m_device, &reg, 1, &value, 1, s_timeoutMs );
}

esp_err_t Axp2101::writeRegister( uint8_t reg, uint8_t value )
{
    const uint8_t data[2] = { reg, value };
    return i2c_master_transmit( m_device, data, sizeof( data ), s_timeoutMs );
}

esp_err_t Axp2101::readAdc14( uint8_t highReg, uint16_t &value )
{
    uint8_t high = 0;
    uint8_t low = 0;
    esp_err_t err = readRegister( highReg, high );
    if ( err == ESP_OK ) {
        err = readRegister( highReg + 1, low );
    }
    value = static_cast<uint16_t>( ( ( high & 0x3F ) << 8 ) | low );
    return err;
}

static const char *chargeStateName( uint8_t status2 )
{
    switch ( status2 & 0x07 ) {
    case 0: return "trickle";
    case 1: return "pre-charge";
    case 2: return "constant current";
    case 3: return "constant voltage";
    case 4: return "done";
    case 5: return "not charging";
    default: return "reserved";
    }
}

static const char *currentDirectionName( uint8_t status2 )
{
    switch ( ( status2 >> 5 ) & 0x03 ) {
    case 0: return "standby";
    case 1: return "charge";
    case 2: return "discharge";
    default: return "reserved";
    }
}

void Axp2101::logStatus()
{
    uint8_t status1 = 0;
    uint8_t status2 = 0;
    if ( readRegister( REG_STATUS1, status1 ) != ESP_OK || readRegister( REG_STATUS2, status2 ) != ESP_OK ) {
        ESP_LOGE( TAG, "Status read failed" );
        return;
    }
    ESP_LOGI( TAG, "STATUS1=0x%02X: VBUS %s, battery %s", status1,
              ( status1 & ( 1 << 5 ) ) ? "good" : "absent", ( status1 & ( 1 << 3 ) ) ? "present" : "absent" );
    ESP_LOGI( TAG, "STATUS2=0x%02X: current %s, charger %s", status2,
              currentDirectionName( status2 ), chargeStateName( status2 ) );

    uint8_t value = 0;
    if ( readRegister( REG_PWRON_STATUS, value ) == ESP_OK ) {
        ESP_LOGI( TAG, "PWRON status (REG20)=0x%02X", value );
    }
    // Reason of the last power-off; kept while a battery is present.
    if ( readRegister( REG_PWROFF_STATUS, value ) == ESP_OK ) {
        ESP_LOGI( TAG, "PWROFF status (REG21)=0x%02X%s", value, ( value & 0x01 ) ? ": PWR key" : "" );
    }
    if ( readRegister( REG_SLEEP_WAKEUP, value ) == ESP_OK ) {
        ESP_LOGI( TAG, "Sleep/wakeup (REG26)=0x%02X", value );
    }
    if ( readRegister( REG_KEY_TIMING, value ) == ESP_OK ) {
        static const char *irqLevel[] = { "1 s", "1.5 s", "2 s", "2.5 s" };
        static const char *offLevel[] = { "4 s", "6 s", "8 s", "10 s" };
        static const char *onLevel[] = { "128 ms", "512 ms", "1 s", "2 s" };
        ESP_LOGI( TAG, "Key timing (REG27)=0x%02X: long press %s, power off %s, power on %s", value,
                  irqLevel[( value >> 4 ) & 0x03], offLevel[( value >> 2 ) & 0x03], onLevel[value & 0x03] );
    }
    if ( readRegister( REG_COMMON_CONFIG, value ) == ESP_OK ) {
        // Bit5 discharges a rail when it is switched off; it keeps DSI_PWR_EN low with ALDO2 off.
        ESP_LOGI( TAG, "Common config (REG10)=0x%02X: off-discharge %s", value, ( value & 0x20 ) ? "on" : "off" );
    }
    if ( readRegister( REG_MODULE_ENABLE, value ) == ESP_OK ) {
        // The button-battery charger (bit2) can draw current from VSYS although no backup cell is fitted.
        uint8_t backupStop = 0;
        readRegister( REG_BACKUP_CHARGE_VOLTAGE, backupStop );
        ESP_LOGI( TAG, "Module enable (REG18)=0x%02X: gauge %s, backup charger %s (REG6A=0x%02X), cell charger %s, "
                       "watchdog %s",
                  value, ( value & 0x08 ) ? "on" : "off", ( value & 0x04 ) ? "on" : "off", backupStop,
                  ( value & 0x02 ) ? "on" : "off", ( value & 0x01 ) ? "on" : "off" );
    }
    uint8_t charge[4] = {};
    if ( readRegister( REG_PRECHARGE_CURRENT, charge[0] ) == ESP_OK &&
         readRegister( REG_CHARGE_CURRENT, charge[1] ) == ESP_OK &&
         readRegister( REG_TERMINATION_CURRENT, charge[2] ) == ESP_OK &&
         readRegister( REG_CHARGE_VOLTAGE, charge[3] ) == ESP_OK ) {
        // Decoding per datasheet §6.13.2.60-63. A termination current that is large for the cell
        // ends the constant-voltage phase early and leaves the cell partly charged.
        const int code = charge[1] & 0x1F;
        const int chargeMa = code <= 8 ? 25 * code : 200 + 100 * ( code - 8 );
        static const char *const voltage[8] = { "reserved", "4.0", "4.1", "4.2", "4.35", "4.4", "reserved", "reserved" };
        ESP_LOGI( TAG,
                  "Charger (REG61-64)=0x%02X 0x%02X 0x%02X 0x%02X: precharge %d mA, charge %d mA, termination %s "
                  "%d mA, target %s V",
                  charge[0], charge[1], charge[2], charge[3], 25 * ( charge[0] & 0x0F ), chargeMa,
                  ( charge[2] & 0x10 ) ? "on" : "off", 25 * ( charge[2] & 0x0F ), voltage[charge[3] & 0x07] );
    }
    if ( readRegister( REG_ADC_ENABLE, value ) == ESP_OK ) {
        ESP_LOGI( TAG, "ADC enable (REG30)=0x%02X", value );
    }
    if ( readRegister( REG_TS_CONTROL, value ) == ESP_OK ) {
        ESP_LOGI( TAG, "TS pin control (REG50)=0x%02X", value );
    }
    uint8_t irq[3] = {};
    if ( readRegister( REG_IRQ_ENABLE0, irq[0] ) == ESP_OK && readRegister( REG_IRQ_ENABLE1, irq[1] ) == ESP_OK &&
         readRegister( REG_IRQ_ENABLE2, irq[2] ) == ESP_OK ) {
        ESP_LOGI( TAG, "IRQ enable (REG40-42)=0x%02X 0x%02X 0x%02X", irq[0], irq[1], irq[2] );
    }
    if ( readRegister( REG_IRQ_STATUS0, irq[0] ) == ESP_OK && readRegister( REG_IRQ_STATUS1, irq[1] ) == ESP_OK &&
         readRegister( REG_IRQ_STATUS2, irq[2] ) == ESP_OK ) {
        ESP_LOGI( TAG, "IRQ status (REG48-4A)=0x%02X 0x%02X 0x%02X", irq[0], irq[1], irq[2] );
    }
    if ( readRegister( REG_DCDC_PWM, value ) == ESP_OK ) {
        ESP_LOGI( TAG, "DCDC PWM control (REG81)=0x%02X: DCDC1 %s", value, ( value & 0x04 ) ? "always PWM" : "auto PWM/PFM" );
    }

    uint8_t dcdcEnable = 0;
    uint8_t dcdc1 = 0;
    if ( readRegister( REG_DCDC_ENABLE, dcdcEnable ) == ESP_OK && readRegister( REG_DCDC1_VOLTAGE, dcdc1 ) == ESP_OK ) {
        // DCDC1: 1.5 V + 100 mV per step (datasheet REG82).
        ESP_LOGI( TAG, "DCDC enable (REG80)=0x%02X, DCDC1 %s %d mV", dcdcEnable,
                  ( dcdcEnable & 0x01 ) ? "on" : "off", 1500 + ( dcdc1 & 0x1F ) * 100 );
    }

    uint8_t ldoEnable0 = 0;
    uint8_t ldoEnable1 = 0;
    if ( readRegister( REG_LDO_ENABLE0, ldoEnable0 ) == ESP_OK && readRegister( REG_LDO_ENABLE1, ldoEnable1 ) == ESP_OK ) {
        ESP_LOGI( TAG, "LDO enable (REG90)=0x%02X (REG91)=0x%02X", ldoEnable0, ldoEnable1 );
        for ( int i = 0; i < 4; ++i ) {
            uint8_t voltage = 0;
            if ( readRegister( REG_ALDO1_VOLTAGE + i, voltage ) == ESP_OK ) {
                // ALDOx: 0.5 V + 100 mV per step (datasheet REG92-95).
                ESP_LOGI( TAG, "ALDO%d %s %d mV", i + 1, ( ldoEnable0 & ( 1 << i ) ) ? "on " : "off",
                          500 + ( voltage & 0x1F ) * 100 );
            }
        }
    }

    uint16_t vbat = 0;
    uint8_t percent = 0;
    if ( readAdc14( REG_VBAT_HIGH, vbat ) == ESP_OK && readRegister( REG_BATTERY_PERCENT, percent ) == ESP_OK ) {
        ESP_LOGI( TAG, "Battery %u mV, %u %%", vbat, percent );
    }
}

esp_err_t Axp2101::readBattery( uint16_t &millivolts, uint8_t &percent, bool &vbusGood )
{
    uint8_t status1 = 0;
    esp_err_t err = readAdc14( REG_VBAT_HIGH, millivolts );
    if ( err == ESP_OK ) {
        err = readRegister( REG_BATTERY_PERCENT, percent );
    }
    if ( err == ESP_OK ) {
        err = readRegister( REG_STATUS1, status1 );
    }
    vbusGood = ( status1 & ( 1 << 5 ) ) != 0;
    return err;
}

const char *Axp2101::chargeState()
{
    uint8_t status2 = 0;
    if ( readRegister( REG_STATUS2, status2 ) != ESP_OK ) {
        return "unknown";
    }
    return chargeStateName( status2 );
}

// Read-modify-write: only the named enable bits change.
static const uint8_t DCDC_OFF_MASK = 0x0E;    // DCDC2-4 (DCDC1 stays on)
static const uint8_t LDO0_KEEP_MASK = 0x01;   // ALDO1 (all other REG90 rails go off)
static const uint8_t LDO1_OFF_MASK = 0x01;    // DLDO2

esp_err_t Axp2101::disableUnusedRails()
{
    uint8_t dcdc = 0;
    uint8_t ldo0 = 0;
    uint8_t ldo1 = 0;
    esp_err_t err = readRegister( REG_DCDC_ENABLE, dcdc );
    if ( err == ESP_OK ) {
        err = writeRegister( REG_DCDC_ENABLE, dcdc & static_cast<uint8_t>( ~DCDC_OFF_MASK ) );
    }
    if ( err == ESP_OK ) {
        err = readRegister( REG_LDO_ENABLE0, ldo0 );
    }
    if ( err == ESP_OK ) {
        err = writeRegister( REG_LDO_ENABLE0, ldo0 & LDO0_KEEP_MASK );
    }
    if ( err == ESP_OK ) {
        err = readRegister( REG_LDO_ENABLE1, ldo1 );
    }
    if ( err == ESP_OK ) {
        err = writeRegister( REG_LDO_ENABLE1, ldo1 & static_cast<uint8_t>( ~LDO1_OFF_MASK ) );
    }
    if ( err != ESP_OK ) {
        ESP_LOGE( TAG, "Rail write failed: %s", esp_err_to_name( err ) );
        return err;
    }

    readRegister( REG_DCDC_ENABLE, dcdc );
    readRegister( REG_LDO_ENABLE0, ldo0 );
    readRegister( REG_LDO_ENABLE1, ldo1 );
    ESP_LOGI( TAG, "Rails after: REG80=0x%02X REG90=0x%02X REG91=0x%02X", dcdc, ldo0, ldo1 );
    const bool expected = ( dcdc & DCDC_OFF_MASK ) == 0 && ( dcdc & 0x01 ) && ldo0 == LDO0_KEEP_MASK &&
                          ( ldo1 & LDO1_OFF_MASK ) == 0;
    return expected ? ESP_OK : ESP_ERR_INVALID_STATE;
}

esp_err_t Axp2101::setPanelPower( bool on )
{
    const uint8_t aldo2 = 1 << 1;
    uint8_t value = 0;
    esp_err_t err = readRegister( REG_LDO_ENABLE0, value );
    if ( err == ESP_OK ) {
        err = writeRegister( REG_LDO_ENABLE0, on ? ( value | aldo2 ) : ( value & static_cast<uint8_t>( ~aldo2 ) ) );
    }
    if ( err == ESP_OK ) {
        err = readRegister( REG_LDO_ENABLE0, value );
    }
    if ( err == ESP_OK && ( ( value & aldo2 ) != 0 ) != on ) {
        err = ESP_ERR_INVALID_STATE;
    }
    ESP_LOGI( TAG, "Panel power (ALDO2) %s: REG90=0x%02X %s", on ? "on" : "off", value, esp_err_to_name( err ) );
    return err;
}

esp_err_t Axp2101::setMeasurement( bool on )
{
    // REG30 0x03 is this board's power-on value: VBAT and TS channels (docs/hardware.md).
    esp_err_t err = writeRegister( REG_ADC_ENABLE, on ? 0x03 : 0x00 );
    uint8_t module = 0;
    if ( err == ESP_OK ) {
        err = readRegister( REG_MODULE_ENABLE, module );
    }
    if ( err == ESP_OK ) {
        const uint8_t gaugeEnable = 0x08;
        err = writeRegister( REG_MODULE_ENABLE, on ? ( module | gaugeEnable ) : ( module & ~gaugeEnable ) );
    }
    return err;
}

esp_err_t Axp2101::setDcdc1Voltage( int millivolts )
{
    if ( millivolts < 1500 || millivolts > 3400 ) {
        return ESP_ERR_INVALID_ARG;
    }
    return writeRegister( REG_DCDC1_VOLTAGE, static_cast<uint8_t>( ( millivolts - 1500 ) / 100 ) );
}

esp_err_t Axp2101::setSleepMode( bool on )
{
    const uint8_t sleepEnable = 0x01;
    uint8_t before = 0;
    esp_err_t err = readRegister( REG_SLEEP_WAKEUP, before );
    if ( err == ESP_OK ) {
        const uint8_t value = on ? ( before | sleepEnable ) : ( before & static_cast<uint8_t>( ~sleepEnable ) );
        err = writeRegister( REG_SLEEP_WAKEUP, value );
    }
    uint8_t after = 0;
    if ( err == ESP_OK ) {
        err = readRegister( REG_SLEEP_WAKEUP, after );
    }
    ESP_LOGI( TAG, "Sleep mode %s: REG26 0x%02X -> 0x%02X %s", on ? "on" : "off", before, after, esp_err_to_name( err ) );
    return err;
}

esp_err_t Axp2101::limitIrqs()
{
    // Status bits are write-1-to-clear (datasheet REG48-4A).
    const uint8_t values[6][2] = {
        { REG_IRQ_ENABLE0, 0x00 },
        { REG_IRQ_ENABLE1, IRQ1_KEY_SHORT | IRQ1_KEY_LONG },
        { REG_IRQ_ENABLE2, 0x00 },
        { REG_IRQ_STATUS0, 0xFF },
        { REG_IRQ_STATUS1, 0xFF },
        { REG_IRQ_STATUS2, 0xFF },
    };
    esp_err_t err = ESP_OK;
    for ( int i = 0; i < 6 && err == ESP_OK; ++i ) {
        err = writeRegister( values[i][0], values[i][1] );
    }
    uint8_t enable[3] = {};
    uint8_t status[3] = {};
    for ( int i = 0; i < 3 && err == ESP_OK; ++i ) {
        err = readRegister( REG_IRQ_ENABLE0 + i, enable[i] );
        if ( err == ESP_OK ) {
            err = readRegister( REG_IRQ_STATUS0 + i, status[i] );
        }
    }
    ESP_LOGI( TAG, "IRQs after: enable 0x%02X 0x%02X 0x%02X, status 0x%02X 0x%02X 0x%02X %s", enable[0], enable[1],
              enable[2], status[0], status[1], status[2], esp_err_to_name( err ) );
    if ( err != ESP_OK ) {
        return err;
    }
    const bool expected = enable[0] == 0x00 && enable[1] == ( IRQ1_KEY_SHORT | IRQ1_KEY_LONG ) && enable[2] == 0x00;
    return expected ? ESP_OK : ESP_ERR_INVALID_STATE;
}

esp_err_t Axp2101::clearPowerKeyIrq( bool &shortPress, bool &longPress )
{
    uint8_t status = 0;
    esp_err_t err = readRegister( REG_IRQ_STATUS1, status );
    if ( err != ESP_OK ) {
        return err;
    }
    shortPress = ( status & IRQ1_KEY_SHORT ) != 0;
    longPress = ( status & IRQ1_KEY_LONG ) != 0;
    // Write 1 only to the key bits so that other IRQ flags are left untouched.
    return writeRegister( REG_IRQ_STATUS1, status & ( IRQ1_KEY_SHORT | IRQ1_KEY_LONG ) );
}

esp_err_t Axp2101::setPowerOnPress()
{
    const uint8_t onLevelMask = 0x03;
    const uint8_t onLevel512ms = 0x01;
    uint8_t before = 0;
    esp_err_t err = readRegister( REG_KEY_TIMING, before );
    uint8_t after = before;
    if ( err == ESP_OK && ( before & onLevelMask ) != onLevel512ms ) {
        err = writeRegister( REG_KEY_TIMING, ( before & ~onLevelMask ) | onLevel512ms );
        if ( err == ESP_OK ) {
            err = readRegister( REG_KEY_TIMING, after );
        }
    }
    ESP_LOGI( TAG, "Power-on press (REG27) 0x%02X -> 0x%02X %s", before, after, esp_err_to_name( err ) );
    return err;
}

esp_err_t Axp2101::setChargeCurrents()
{
    // REG61 25 mA per step; REG62 code 4 = 100 mA; REG63 bit4 keeps termination on, 25 mA per step.
    const uint8_t values[3][2] = {
        { REG_PRECHARGE_CURRENT, 0x01 },
        { REG_CHARGE_CURRENT, 0x04 },
        { REG_TERMINATION_CURRENT, 0x11 },
    };
    esp_err_t err = ESP_OK;
    for ( int i = 0; i < 3 && err == ESP_OK; ++i ) {
        err = writeRegister( values[i][0], values[i][1] );
    }
    return err;
}

esp_err_t Axp2101::powerOff()
{
    const uint8_t softPowerOff = 0x01;
    uint8_t value = 0;
    esp_err_t err = readRegister( REG_COMMON_CONFIG, value );
    if ( err == ESP_OK ) {
        err = writeRegister( REG_COMMON_CONFIG, value | softPowerOff );
    }
    if ( err == ESP_OK ) {
        // The rails fall within milliseconds; still running after this means the write had no effect.
        vTaskDelay( pdMS_TO_TICKS( 500 ) );
        err = ESP_FAIL;
    }
    return err;
}
