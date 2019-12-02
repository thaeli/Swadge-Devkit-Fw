/*==============================================================================
 * Includes
 *============================================================================*/

#include <osapi.h>
#include "i2c_master.h"
#include "soft_i2c.h"

/*==============================================================================
 * Prototypes
 *============================================================================*/

bool ICACHE_FLASH_ATTR soft_i2c_write_check_ack(uint8_t byte);

/*==============================================================================
 * Variables
 *============================================================================*/

uint8_t slaveAddr = 0;
bool soft_i2c_error = false;

/*==============================================================================
 * Functions
 *============================================================================*/

/**
 * @brief Initializer for software I2C
 *
 * @param clock_stretch_time_out_usec unused
 */
void ICACHE_FLASH_ATTR soft_i2c_setup(uint32_t clock_stretch_time_out_usec __attribute__((unused)))
{
    slaveAddr = 0;
    soft_i2c_error = false;
    i2c_master_gpio_init();
}

/**
 * @brief Start a transaction to the given address
 *
 * @param slave_address The address to write to or read from
 * @param SCL_frequency_KHz  unused
 */
void ICACHE_FLASH_ATTR soft_i2c_start_transaction(uint8_t slave_address,
        uint16_t SCL_frequency_KHz __attribute__((unused)))
{
    slaveAddr = slave_address;
    soft_i2c_error = false;
}

/**
 * @brief Write a series of bytes over I2C to the address specified in soft_i2c_start_transaction()
 *
 * @param data A pointer to bytes to write
 * @param no_of_bytes The number of bytes to write
 * @param repeated_start unused
 */
void ICACHE_FLASH_ATTR soft_i2c_write(const uint8_t* data, uint32_t no_of_bytes,
                                      bool repeated_start __attribute__((unused)))
{
    i2c_master_start();
    if(false == soft_i2c_write_check_ack((slaveAddr << 1 ) | 0))
    {
        soft_i2c_error = true;
        return;
    }

    while(no_of_bytes--)
    {
        if(false == soft_i2c_write_check_ack(*(data++)))
        {
            soft_i2c_error = true;
            return;
        }
    }
    i2c_master_stop();
}

/**
 * @brief Read a series of bytes over I2C from the address specified in soft_i2c_start_transaction()
 *
 * @param data A pointer to store the bytes which are read
 * @param nr_of_bytes The number of bytes to read
 * @param repeated_start unused
 */
void ICACHE_FLASH_ATTR soft_i2c_read(uint8_t* data, uint32_t nr_of_bytes, bool repeated_start __attribute__((unused)))
{
    i2c_master_start();
    if(false == soft_i2c_write_check_ack((slaveAddr << 1 ) | 1))
    {
        soft_i2c_error = true;
        return;
    }

    uint32_t i;
    for (i = 0; i < nr_of_bytes; i++)
    {
        data[i] = i2c_master_readByte();
        i2c_master_setAck((i == (nr_of_bytes - 1)) ? 1 : 0);
    }
    i2c_master_stop();
}

/**
 * @brief End the current I2C transaction
 *
 * @return uint8_t 0 for no errors, 1 for an error
 */
uint8_t ICACHE_FLASH_ATTR soft_i2c_end_transaction(void)
{
    return (soft_i2c_error == false) ? 0 : 1;
}

/**
 * @brief Helper function to write a single byte and handle the ack
 *
 * @param byte The byte to write
 * @return true  if the byte was acked
 * @return false if it was not
 */
bool ICACHE_FLASH_ATTR soft_i2c_write_check_ack(uint8_t byte)
{
    i2c_master_writeByte(byte);
    if (i2c_master_getAck())
    {
        i2c_master_stop();
        return false;
    }
    return true;
}
