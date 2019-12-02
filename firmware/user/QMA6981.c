/*
 * QMA6981.c
 *
 *  Created on: Jul 9, 2019
 *      Author: adam
 */

// Datasheet at
// https://datasheet.lcsc.com/szlcsc/QST-QMA6981_C310611.pdf
// Some values taken from
// https://github.com/yangzhiqiang723/rainbow-RB59M325ALB/blob/02ea6fc2a7f9744273b850cff751ffd2fcf1820b/src/QMA6981.c
// https://github.com/yangzhiqiang723/rainbow-RB59M325ALB/blob/02ea6fc2a7f9744273b850cff751ffd2fcf1820b/inc/QMA6981.h

#include <osapi.h>
#include "user_main.h"
#include "soft_i2c.h"
#include "QMA6981.h"

#define QMA6981_ADDR 0x12
#define QMA6981_FREQ  400

/*============================================================================
 * Register addresses and definitions
 *==========================================================================*/

/* For the bandwidth register */

typedef enum
{
    QMA6981_BW_3_9  = 0b000,
    QMA6981_BW_7_8  = 0b001,
    QMA6981_BW_15_6 = 0b010,
    QMA6981_BW_31_2 = 0b011,
    QMA6981_BW_62_5 = 0b100,
    QMA6981_BW_125  = 0b101,
    QMA6981_BW_250  = 0b110,
    QMA6981_BW_500  = 0b111,
} QMA6981_BANDWIDTH;

typedef union
{
    uint8_t val;
    struct
    {
        QMA6981_BANDWIDTH BW : 5;
        bool ODRH            : 1;
    } bitmask;
} QMA6981_BW_VAL;

/* For the range register */

enum
{
    QMA6981_RANGE_2G  = 0x01,
    QMA6981_RANGE_4G  = 0x02,
    QMA6981_RANGE_8G  = 0x04,
};

/* For interrupt configuration register */

typedef union
{
    uint8_t val;
    struct
    {
        bool res               : 1;
        bool STEP_UNSIMILAR_EN : 1;
        bool STEP_QUIT_EN      : 1;
        bool STEP_EN           : 1;
        bool D_TAP_EN          : 1;
        bool S_TAP_EN          : 1;
        bool ORIENT_EN         : 1;
        bool FOB_EN            : 1;
    } bitmask;
} QMA6981_INT_EN0_VAL;

/* For the power level value register */

typedef enum
{
    Tpreset_12us   = 0b00,
    Tpreset_96us   = 0b01,
    Tpreset_768us  = 0b10,
    Tpreset_2048us = 0b11
} QMA6981_POWER_VAL_PRESET;

typedef enum
{
    SLEEP_DUR_FULL_SPEED = 0b0000,
    SLEEP_DUR_0_5ms      = 0b0001,
    SLEEP_DUR_1ms        = 0b0110,
    SLEEP_DUR_2ms        = 0b0111,
    SLEEP_DUR_4ms        = 0b1000,
    SLEEP_DUR_6ms        = 0b1001,
    SLEEP_DUR_10ms       = 0b1010,
    SLEEP_DUR_25ms       = 0b1011,
    SLEEP_DUR_50ms       = 0b1100,
    SLEEP_DUR_100ms      = 0b1101,
    SLEEP_DUR_500ms      = 0b1110,
    SLEEP_DUR_1000ms     = 0b1111,
} QMA6981_POWER_VAL_SLEEP_DUR;

typedef union
{
    uint8_t val;
    struct
    {
        QMA6981_POWER_VAL_SLEEP_DUR SLEEP_DUR : 4;
        QMA6981_POWER_VAL_PRESET PRESET       : 2;
        bool res                              : 1;
        bool MODE_BIT                         : 1;
    } bitmask;
} QMA6981_POWER_VAL;

/* Soft reset register value */

#define QMA6981_SOFT_RESET_ALL_REGISTERS 0xB6

/* Register addresses */

typedef enum
{
    QMA6981_CHIP_ID      = 0x00,
    QMA6981_DATA         = 0x01,
    QMA6981_STEP_CNT     = 0x07,
    QMA6981_INT_STATUS   = 0x0A,
    QMA6981_FIFO_STATUS  = 0x0E,
    QMA6981_FULL_SCALE   = 0x0F,
    QMA6981_BW           = 0x10,
    QMA6981_POWER_MODE   = 0x11,
    QMA6981_STEP_CONF    = 0x13,
    QMA6981_INT_EN       = 0x16,
    QMA6981_INT_SRC      = 0x18,
    QMA6981_INT_MAP      = 0x19,
    QMA6981_INT_PIN_CONF = 0x20,
    QMA6981_INT_LATCH    = 0x21,
    QMA6981_LowG_HighG   = 0x22,
    QMA6981_OS_CUST      = 0x27,
    QMA6981_TAP_1        = 0x2A,
    QMA6981_TAP_2        = 0x2B,
    QMA6981__4D_6D       = 0x2C,
    QMA6981_FIFO_WM      = 0x31,
    QMA6981_SelfTest     = 0x32,
    QMA6981_NVM_CFG      = 0x33,
    QMA6981_SOFT_RESET   = 0x36,
    QMA6981_IMAGE        = 0x37,
    QMA6981_FIFO_CONF    = 0x3E,
} QMA6981_reg_addr;

/*============================================================================
 * Function prototypes
 *==========================================================================*/

uint8_t ICACHE_FLASH_ATTR QMA6981_writereg(QMA6981_reg_addr addr, uint8_t data);
uint8_t ICACHE_FLASH_ATTR QMA6981_readreg(QMA6981_reg_addr addr, uint8_t len, uint8_t* data);
int16_t ICACHE_FLASH_ATTR convertTwosComplement10bit(uint16_t in);

/*============================================================================
 * Variables
 *==========================================================================*/

accel_t lastKnownAccel = {0};

/*============================================================================
 * Functions
 *==========================================================================*/

/**
 * @brief Write a single byte of data to the given QMA6981 register
 *
 * @param addr The address to write to
 * @param data The single byte to write
 * @return true if the data was read, false if there was an i2c error
 */
uint8_t ICACHE_FLASH_ATTR QMA6981_writereg(QMA6981_reg_addr addr, uint8_t data)
{
    soft_i2c_start_transaction(QMA6981_ADDR, QMA6981_FREQ);
    uint8_t writeCmd[2] = {addr, data};
    soft_i2c_write(writeCmd, sizeof(writeCmd), false);
    return soft_i2c_end_transaction();
}

/**
 * @brief TODO Read a number of bytes from the given QMA6981 register
 *
 * @param addr The address to read from
 * @param len  The number of bytes to read
 * @param data A pointer to read the bytes into
 * @return true if the data was read, false if there was an i2c error
 */
uint8_t ICACHE_FLASH_ATTR QMA6981_readreg(QMA6981_reg_addr addr, uint8_t len, uint8_t* data)
{
    soft_i2c_start_transaction(QMA6981_ADDR, QMA6981_FREQ);
    uint8_t reg[1] = {addr};
    soft_i2c_write(reg, sizeof(reg), false);
    soft_i2c_read(data, len, false);
    return soft_i2c_end_transaction();
}

/**
 * @brief Initialize the QMA6981 and start it going
 *
 * @return true if initialization succeeded, false if it failed
 */
bool ICACHE_FLASH_ATTR QMA6981_setup(void)
{
    QMA6981_POWER_VAL active =
    {
        .bitmask.MODE_BIT = true,
        .bitmask.res = true,
        .bitmask.SLEEP_DUR = SLEEP_DUR_FULL_SPEED,
        .bitmask.PRESET = Tpreset_12us
    };
    if(0 != QMA6981_writereg(QMA6981_POWER_MODE, active.val))
    {
        return false;
    }

    if(0 != QMA6981_writereg(QMA6981_SOFT_RESET, QMA6981_SOFT_RESET_ALL_REGISTERS))
    {
        return false;
    }
    if(0 != QMA6981_writereg(QMA6981_POWER_MODE, active.val))
    {
        return false;
    }
    os_delay_us(5);

    QMA6981_BW_VAL bandwidth =
    {
        .bitmask.ODRH = false,
        .bitmask.BW = QMA6981_BW_31_2
    };
    if(0 != QMA6981_writereg(QMA6981_BW, bandwidth.val))
    {
        return false;
    }

    if(0 != QMA6981_writereg(QMA6981_FULL_SCALE, QMA6981_RANGE_2G))
    {
        return false;
    }

    if(0 != QMA6981_writereg(QMA6981_POWER_MODE, active.val))
    {
        return false;
    }

    return true;
}

/**
 * @brief Poll the QMA6981 for the current acceleration value
 *
 * @param currentAccel A pointer where the acceleration data will be stored
 */
void ICACHE_FLASH_ATTR QMA6981_poll(accel_t* currentAccel)
{
    // Read 7 bytes of data(0x00)
    uint8_t raw_data[6];
    if(0 != QMA6981_readreg(QMA6981_DATA, 6, raw_data))
    {
        // os_printf("read xyz error!!!\n");
        // Try reinitializing, then return last known value
        QMA6981_setup();
    }
    else
    {
        // Convert the data to 12-bits, save it as the last known value
        lastKnownAccel.x = convertTwosComplement10bit(((raw_data[0] >> 6 ) | (raw_data[1]) << 2) & 0x03FF);
        lastKnownAccel.y = convertTwosComplement10bit(((raw_data[2] >> 6 ) | (raw_data[3]) << 2) & 0x03FF);
        lastKnownAccel.z = convertTwosComplement10bit(((raw_data[4] >> 6 ) | (raw_data[5]) << 2) & 0x03FF);
    }

    // Copy out the acceleration value
    currentAccel->x = lastKnownAccel.x;
    currentAccel->y = lastKnownAccel.y;
    currentAccel->z = lastKnownAccel.z;
}

/**
 * @brief Helper function to convert a 10 bit 2's complement number to 16 bit
 *
 * @param in
 * @return int16_t convertTwosComplement10bit
 */
int16_t ICACHE_FLASH_ATTR convertTwosComplement10bit(uint16_t in)
{
    if(in & 0x200)
    {
        return (in | 0xFC00); // extend the sign bit all the way out
    }
    else
    {
        return (in & 0x01FF); // make sure the sign bits are cleared
    }
}
