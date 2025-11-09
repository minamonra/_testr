#include "eeprom.h"
#include "stm32f10x.h"
#include <string.h>

#define EEPROM_ADDR 0xA0       // Адрес EEPROM 24C32
#define EEPROM_SIZE 4096
#define STRING_SIZE 32          // Длина строки
#define MAX_STRINGS 100         // Кол-во строк
#define VAR_UINT16_COUNT 20
#define PAGE_SIZE 16            // Страница EEPROM

#ifndef NULL
#define NULL ((void*)0)
#endif

// Простейшая программная задержка
static void delay_simple(uint32_t count) {
    while(count--) { __NOP(); }
}

// --- Чтение одного байта ---
int eeprom_read_byte(uint16_t addr, uint8_t *data) {
    if (!data) return -1;

    while (I2C1->SR2 & I2C_SR2_BUSY);

    I2C1->CR1 |= I2C_CR1_START;
    uint32_t to = 100000; while (!(I2C1->SR1 & I2C_SR1_SB)) { if (--to==0) return -1; }

    I2C1->DR = EEPROM_ADDR; // write
    to = 100000; while (!(I2C1->SR1 & I2C_SR1_ADDR)) { if (--to==0) return -1; }
    (void)I2C1->SR2;

    I2C1->DR = (addr >> 8) & 0xFF;
    to = 100000; while (!(I2C1->SR1 & I2C_SR1_TXE)) { if (--to==0) return -1; }

    I2C1->DR = addr & 0xFF;
    to = 100000; while (!(I2C1->SR1 & I2C_SR1_TXE)) { if (--to==0) return -1; }

    I2C1->CR1 |= I2C_CR1_START; // repeated START
    to = 100000; while (!(I2C1->SR1 & I2C_SR1_SB)) { if (--to==0) return -1; }

    I2C1->DR = EEPROM_ADDR | 1; // read
    to = 100000; while (!(I2C1->SR1 & I2C_SR1_ADDR)) { if (--to==0) return -1; }
    (void)I2C1->SR1; (void)I2C1->SR2;

    I2C1->CR1 &= ~I2C_CR1_ACK;
    I2C1->CR1 |= I2C_CR1_STOP;

    to = 100000; while (!(I2C1->SR1 & I2C_SR1_RXNE)) { if (--to==0) return -1; }
    *data = I2C1->DR;

    I2C1->CR1 |= I2C_CR1_ACK;
    return 0;
}

// --- Запись одного байта ---
int eeprom_write_byte(uint16_t addr, uint8_t data) {
    while (I2C1->SR2 & I2C_SR2_BUSY);

    I2C1->CR1 |= I2C_CR1_START;
    uint32_t to = 100000; while (!(I2C1->SR1 & I2C_SR1_SB)) { if (--to==0) return -1; }

    I2C1->DR = EEPROM_ADDR;
    to = 100000; while (!(I2C1->SR1 & I2C_SR1_ADDR)) { if (--to==0) return -1; }
    (void)I2C1->SR2;

    I2C1->DR = (addr >> 8) & 0xFF;
    to = 100000; while (!(I2C1->SR1 & I2C_SR1_TXE)) { if (--to==0) return -1; }

    I2C1->DR = addr & 0xFF;
    to = 100000; while (!(I2C1->SR1 & I2C_SR1_TXE)) { if (--to==0) return -1; }

    I2C1->DR = data;
    to = 100000; while (!(I2C1->SR1 & I2C_SR1_TXE)) { if (--to==0) return -1; }

    I2C1->CR1 |= I2C_CR1_STOP;
    delay_simple(50000);
    return 0;
}

// --- Постраничное чтение строки ---
int eeprom_read_string_by_num(uint16_t string_num, char *str) {
    if (!str) return -1;
    if (string_num >= MAX_STRINGS) return -2;

    uint16_t addr = string_num * STRING_SIZE;
    uint16_t i;
    uint8_t data;
    int all_ff = 1;

    for (i = 0; i < STRING_SIZE; i++) {
        // Проверка адреса на предел EEPROM
        if (addr + i >= EEPROM_SIZE) {
            str[i] = '\0';
            break;
        }

        // --- Чтение байта ---
        if (eeprom_read_byte(addr + i, &data) != 0) {
            str[i] = '\0';
            return -1; // Ошибка I2C
        }

        if (data != 0xFF) all_ff = 0;
        str[i] = (data == 0xFF) ? ' ' : (char)data;

        if (data == '\0') break;
    }

    // Завершаем строку нулём
    if (i == STRING_SIZE) str[STRING_SIZE - 1] = '\0';

    // Если вся строка 0xFF, заполняем пробелами
    if (all_ff) {
        for (i = 0; i < STRING_SIZE - 1; i++) str[i] = ' ';
        str[STRING_SIZE - 1] = '\0';
    }

    return 0;
}


// --- Постраничная запись строки ---
int eeprom_write_string_by_num(uint16_t string_num, const char *str) {
    if (!str) return -1;
    if (string_num >= MAX_STRINGS) return -2;

    uint16_t addr = string_num * STRING_SIZE;
    uint16_t written = 0;

    while (written < STRING_SIZE) {
        uint16_t page_offset = addr % PAGE_SIZE;
        uint16_t bytes_to_page_end = PAGE_SIZE - page_offset;
        uint16_t bytes_left = STRING_SIZE - written;
        uint16_t n = (bytes_left < bytes_to_page_end) ? bytes_left : bytes_to_page_end;

        for (uint16_t i = 0; i < n; i++) {
            uint8_t b = (written < strlen(str)) ? (uint8_t)str[written] : 0;
            if (eeprom_write_byte(addr + i, b) != 0) return -1;
            written++;
        }
        addr += n;
    }
    return 0;
}

// --- Очистка строки ---
int eeprom_clear_string(uint16_t string_num) {
    if (string_num >= MAX_STRINGS) return -2;

    uint16_t addr = string_num * STRING_SIZE;
    for (uint16_t i=0;i<STRING_SIZE;i++)
        if (eeprom_write_byte(addr+i, 0xFF) != 0) return -1;
    return 0;
}

// --- Проверка использования строки ---
int eeprom_is_string_used(uint16_t string_num) {
    if (string_num >= MAX_STRINGS) return -2;

    uint16_t addr = string_num * STRING_SIZE;
    uint8_t byte;
    for (uint16_t i=0;i<STRING_SIZE;i++) {
        if (eeprom_read_byte(addr+i, &byte) != 0) return -1;
        if (byte != 0xFF) return 1;
    }
    return 0;
}

// --- Очистка всех строк ---
int eeprom_clear_all_strings(void) {
    for (uint16_t i=0;i<MAX_STRINGS;i++)
        if (eeprom_clear_string(i) != 0) return -1;
    return 0;
}

// --- Работа с uint16_t переменными ---
#define VARS_START_ADDR (MAX_STRINGS*STRING_SIZE)

int eeprom_write_uint16_by_num(uint16_t var_num, uint16_t value) {
    if (var_num==0 || var_num>VAR_UINT16_COUNT) return -2;
    uint16_t addr = VARS_START_ADDR + (var_num-1)*2;
    if (eeprom_write_byte(addr, value & 0xFF) != 0) return -1;
    if (eeprom_write_byte(addr+1, (value>>8) & 0xFF) != 0) return -1;
    return 0;
}

int eeprom_read_uint16_by_num(uint16_t var_num, uint16_t *value) {
    if (!value) return -1;
    if (var_num==0 || var_num>VAR_UINT16_COUNT) return -2;
    uint16_t addr = VARS_START_ADDR + (var_num-1)*2;
    uint8_t low, high;
    if (eeprom_read_byte(addr, &low) != 0) return -1;
    if (eeprom_read_byte(addr+1, &high) != 0) return -1;
    *value = ((uint16_t)high << 8) | low;
    return 0;
}

// --- Очистка всех uint16_t ---
int eeprom_clear_all_uint16_vars(void) {
    for (uint16_t i=1;i<=VAR_UINT16_COUNT;i++)
        if (eeprom_write_uint16_by_num(i, 0xFFFF) != 0) return -1;
    return 0;
}
