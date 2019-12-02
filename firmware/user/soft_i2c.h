#ifndef _soft_i2c_h
#define _soft_i2c_h

void ICACHE_FLASH_ATTR soft_i2c_setup(uint32_t clock_stretch_time_out_usec);
void ICACHE_FLASH_ATTR soft_i2c_start_transaction(uint8_t slave_address, uint16_t SCL_frequency_KHz);
void ICACHE_FLASH_ATTR soft_i2c_write(const uint8_t* data, uint32_t no_of_bytes, bool repeated_start);
void ICACHE_FLASH_ATTR soft_i2c_read(uint8_t* data, uint32_t nr_of_bytes, bool repeated_start);
uint8_t ICACHE_FLASH_ATTR soft_i2c_end_transaction(void);

#endif
