#include "eeprom.h"
#include "stm32f10x.h"

#define EEPROM_SIZE 4096
//#define STRING_SIZE 32          // Изменено с 16 на 32
//#define MAX_STRINGS 100         // Изменено с 200 на 100
#define VAR_UINT16_COUNT 20
#define PAGE_SIZE 32            // Размер страницы для постраничного доступа

#ifndef NULL
#define NULL ((void *)0)
#endif

// Простая программная задержка
void delay_simple(uint32_t count) {
  while (count--) {
    __NOP();
  }
}

int I2C_WaitEvent(uint32_t event) {
  uint32_t timeout = 100000;
  while (!(I2C1->SR1 & event)) {
    if (--timeout == 0)
      return 0;
  }
  return 1;
}

// Запись страницы данных в EEPROM (постранично)
int eeprom_write_page(uint16_t addr, uint8_t *data, uint16_t length) {
  if (length == 0 || length > PAGE_SIZE) return -1;
  
  while (I2C1->SR2 & I2C_SR2_BUSY);

  I2C1->CR1 |= I2C_CR1_START;
  if (!I2C_WaitEvent(I2C_SR1_SB)) return -1;

  I2C1->DR = EEPROM_ADDR;
  if (!I2C_WaitEvent(I2C_SR1_ADDR)) return -1;
  (void)I2C1->SR2;

  I2C1->DR = (addr >> 8) & 0xFF;
  if (!I2C_WaitEvent(I2C_SR1_TXE)) return -1;

  I2C1->DR = addr & 0xFF;
  if (!I2C_WaitEvent(I2C_SR1_TXE)) return -1;

  // Записываем все байты страницы
  for (uint16_t i = 0; i < length; i++) {
    I2C1->DR = data[i];
    if (!I2C_WaitEvent(I2C_SR1_TXE)) return -1;
  }

  I2C1->CR1 |= I2C_CR1_STOP;
  delay_simple(100000);  // Увеличиваем задержку для записи страницы
  return 0;
}

// Чтение страницы данных из EEPROM (постранично)
int eeprom_read_page(uint16_t addr, uint8_t *data, uint16_t length) {
  if (length == 0 || length > PAGE_SIZE) return -1;
  
  while (I2C1->SR2 & I2C_SR2_BUSY);

  I2C1->CR1 |= I2C_CR1_START;
  if (!I2C_WaitEvent(I2C_SR1_SB)) return -1;

  I2C1->DR = EEPROM_ADDR;
  if (!I2C_WaitEvent(I2C_SR1_ADDR)) return -1;
  (void)I2C1->SR2;

  I2C1->DR = (addr >> 8) & 0xFF;
  if (!I2C_WaitEvent(I2C_SR1_TXE)) return -1;

  I2C1->DR = addr & 0xFF;
  if (!I2C_WaitEvent(I2C_SR1_TXE)) return -1;

  I2C1->CR1 |= I2C_CR1_START;
  if (!I2C_WaitEvent(I2C_SR1_SB)) return -1;

  I2C1->DR = EEPROM_ADDR | 1;
  if (!I2C_WaitEvent(I2C_SR1_ADDR)) return -1;
  (void)I2C1->SR2;

  I2C1->CR1 &= ~I2C_CR1_ACK;

  // Читаем все байты страницы
  for (uint16_t i = 0; i < length; i++) {
    if (i == length - 1) {
      I2C1->CR1 &= ~I2C_CR1_ACK;  // Для последнего байта
    } else {
      I2C1->CR1 |= I2C_CR1_ACK;   // Для всех кроме последнего
    }
    
    if (!I2C_WaitEvent(I2C_SR1_RXNE)) return -1;
    data[i] = I2C1->DR;
  }

  I2C1->CR1 |= I2C_CR1_STOP;
  I2C1->CR1 |= I2C_CR1_ACK;
  return 0;
}

// Записывает строку в EEPROM ПОСТРАНИЧНО
int eeprom_write_string(uint16_t addr, const char *str) {
  uint8_t buffer[STRING_SIZE] = {0};
  uint16_t i = 0;

  // Копируем строку в буфер
  while (i < STRING_SIZE - 1 && str[i] != '\0') {
    buffer[i] = (uint8_t)str[i];
    i++;
  }
  buffer[i] = '\0';  // Гарантируем терминатор

  // Записываем всю страницу сразу
  return eeprom_write_page(addr, buffer, STRING_SIZE);
}

// Читает строку из EEPROM ПОСТРАНИЧНО
int eeprom_read_string(uint16_t addr, char *str) {
  uint8_t buffer[STRING_SIZE];
  
  // Читаем всю страницу сразу
  if (eeprom_read_page(addr, buffer, STRING_SIZE) != 0) {
    return -1;
  }

  // Копируем и обрабатываем данные
  for (uint16_t i = 0; i < STRING_SIZE; i++) {
    // Заменяем 0xFF на пробелы
    if (buffer[i] == 0xFF) {
      str[i] = ' ';
    } else {
      str[i] = (char)buffer[i];
    }
    
    // Если встретили терминатор - выходим
    if (buffer[i] == '\0') {
      break;
    }
  }
  
  // Гарантируем терминатор
  if (str[STRING_SIZE - 1] != '\0') {
    str[STRING_SIZE - 1] = '\0';
  }
  
  return 0;
}

// Адрес начала блока переменных uint16_t (теперь 100 * 32 = 3200)
#define VARS_START_ADDR (MAX_STRINGS * STRING_SIZE)    // 3200

// Записывает строку ПОСТРАНИЧНО по номеру строки
int eeprom_write_string_by_num(uint16_t string_num, const char *str) {
  if (string_num >= MAX_STRINGS) return -2;

  uint16_t addr = string_num * STRING_SIZE;
  uint8_t buffer[STRING_SIZE] = {0};
  uint16_t i = 0;

  // Подготавливаем буфер
  while (i < STRING_SIZE - 1 && str[i] != '\0') {
    buffer[i] = (uint8_t)str[i];
    i++;
  }
  buffer[i] = '\0';

  // Записываем страницу целиком
  return eeprom_write_page(addr, buffer, STRING_SIZE);
}

// Читает строку ПОСТРАНИЧНО по номеру строки
int eeprom_read_string_by_num(uint16_t string_num, char *str) {
  if (str == NULL) return -1;
  if (string_num >= MAX_STRINGS) return -2;

  uint16_t addr = string_num * STRING_SIZE;
  uint8_t buffer[STRING_SIZE];

  // Читаем страницу целиком
  if (eeprom_read_page(addr, buffer, STRING_SIZE) != 0) {
    return -1;
  }

  // Обрабатываем данные
  uint16_t i;
  for (i = 0; i < STRING_SIZE - 1; i++) {
    if (buffer[i] == 0xFF) {
      str[i] = ' ';
    } else {
      str[i] = (char)buffer[i];
    }
    
    if (buffer[i] == '\0') break;
  }
  str[i] = '\0';

  return 0;
}

// Очищает строку в EEPROM (записывает всю страницу 0xFF)
int eeprom_clear_string(uint16_t string_num) {
  if (string_num >= MAX_STRINGS) return -2;

  uint16_t addr = string_num * STRING_SIZE;
  uint8_t clear_buffer[STRING_SIZE];
  
  // Заполняем буфер 0xFF
  for (uint16_t i = 0; i < STRING_SIZE; i++) {
    clear_buffer[i] = 0xFF;
  }

  // Записываем страницу очистки
  return eeprom_write_page(addr, clear_buffer, STRING_SIZE);
}

// Проверяет, есть ли данные в строке
int eeprom_is_string_used(uint16_t string_num) {
  if (string_num >= MAX_STRINGS) return -2;

  uint16_t addr = string_num * STRING_SIZE;
  uint8_t buffer[STRING_SIZE];

  // Читаем всю страницу
  if (eeprom_read_page(addr, buffer, STRING_SIZE) != 0) return -1;

  // Проверяем есть ли не-0xFF байты
  for (uint16_t i = 0; i < STRING_SIZE; i++) {
    if (buffer[i] != 0xFF) return 1;
  }
  return 0;
}

// Остальные функции остаются без изменений, но используют новые адреса
int eeprom_clear_all_strings(void) {
  for (uint16_t i = 0; i < MAX_STRINGS; i++) {
    if (eeprom_clear_string(i) != 0) return -1;
  }
  return 0;
}

int eeprom_write_uint16_by_num(uint16_t var_num, uint16_t value) {
  if (var_num == 0 || var_num > VAR_UINT16_COUNT) return -2;

  uint16_t addr = VARS_START_ADDR + (var_num - 1) * 2;

  // Для записи отдельных байтов оставляем побайтовые функции
  uint8_t data[2] = {(uint8_t)(value & 0xFF), (uint8_t)((value >> 8) & 0xFF)};
  return eeprom_write_page(addr, data, 2);
}

int eeprom_read_uint16_by_num(uint16_t var_num, uint16_t *value) {
  if (var_num == 0 || var_num > VAR_UINT16_COUNT) return -2;

  uint16_t addr = VARS_START_ADDR + (var_num - 1) * 2;
  uint8_t data[2];

  if (eeprom_read_page(addr, data, 2) != 0) return -1;

  *value = ((uint16_t)data[1] << 8) | data[0];
  return 0;
}

int eeprom_clear_all_uint16_vars(void) {
  uint8_t clear_data[2] = {0xFF, 0xFF};
  
  for (uint16_t i = 1; i <= VAR_UINT16_COUNT; i++) {
    uint16_t addr = VARS_START_ADDR + (i - 1) * 2;
    if (eeprom_write_page(addr, clear_data, 2) != 0) return -1;
  }
  return 0;
}