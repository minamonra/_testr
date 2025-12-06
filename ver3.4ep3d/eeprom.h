#ifndef __EEPROM_H__
#define __EEPROM_H__
#include <stdint.h>

#define EEPROM_ADDR 0xA0 // Дефолтный адрес 24C32/24C64 без резисторов
// Добавляем прототип функции I2C_WaitEvent, если она определена где-то еще
int I2C_WaitEvent(uint32_t event);


int eeprom_write_byte(uint16_t addr, uint8_t data);
int eeprom_read_byte(uint16_t addr, uint8_t *data);
int eeprom_write_string(uint16_t addr, const char *str);
int eeprom_read_string(uint16_t addr, char *str);
int eeprom_write_string_by_num(uint16_t string_num, const char *str);
int eeprom_read_string_by_num(uint16_t string_num, char *str);
int eeprom_write_uint16_by_num(uint16_t var_num, uint16_t value);
int eeprom_read_uint16_by_num(uint16_t var_num, uint16_t *value);
int eeprom_clear_string(uint16_t string_num);
int eeprom_clear_all_strings(void);
int eeprom_clear_all_uint16_vars(void);


#endif // __EEPROM_H__