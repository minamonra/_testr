#include "eeprom.h"
#include "stm32f10x.h"

#define EEPROM_SIZE 4096
#define STRING_SIZE 16
#define MAX_STRINGS 200
#define VAR_UINT16_COUNT 20

#ifndef NULL
#define NULL ((void *)0)
#endif

// Простая программная задержка (примерно)
void delay_simple(uint32_t count) {
  while (count--) {
    __NOP();    // asm nop для небольшого замедления
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

// Запись байта в EEPROM
int eeprom_write_byte(uint16_t addr, uint8_t data) {
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

  I2C1->DR = data;
  if (!I2C_WaitEvent(I2C_SR1_TXE)) return -1;

  I2C1->CR1 |= I2C_CR1_STOP;
  // Простейшая задержка для записи EEPROM
  delay_simple(50000);
  return 0;
}

// Чтение байта из EEPROM
int eeprom_read_byte(uint16_t addr, uint8_t *data) {
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

  if (!I2C_WaitEvent(I2C_SR1_RXNE)) return -1;

  *data = I2C1->DR;

  I2C1->CR1 |= I2C_CR1_STOP;
  I2C1->CR1 |= I2C_CR1_ACK;

  return 0;
}

// Записывает строку в EEPROM
// addr Начальный адрес для записи
// str Указатель на строку (максимум 15 символов + терминатор)
// ret 0 в случае успеха, -1 при ошибке
int eeprom_write_string(uint16_t addr, const char *str) {
  uint16_t i = 0;

  // Записываем каждый байт строки включая нулевой терминатор
  while (i < 16 && str[i] != '\0') {
    if (eeprom_write_byte(addr + i, (uint8_t)str[i]) != 0) {
      return -1;    // Ошибка записи
    }
    i++;
  }

  // Записываем нулевой терминатор, если строка короче 16 символов
  if (i < 16) {
    if (eeprom_write_byte(addr + i, '\0') != 0) {
      return -1;
    }
  }
  return 0;    // Успех
}

// Читает строку из EEPROM
// addr Начальный адрес для чтения
// str Буфер для строки (минимум 16 байт)
// ret 0 в случае успеха, -1 при ошибке
int eeprom_read_string(uint16_t addr, char *str) {
  uint8_t byte;
  uint16_t i = 0;

  // Читаем байты пока не встретим терминатор или не достигнем максимума
  while (i < 16) {
    if (eeprom_read_byte(addr + i, &byte) != 0) {
      return -1;    // Ошибка чтения
    }
    str[i] = (char)byte;

    // Если встретили нулевой терминатор - выходим
    if (byte == '\0') {
      break;
    }
    i++;
  }

  // Гарантируем, что строка всегда заканчивается терминатором
  if (i == 16) {
    str[15] = '\0';
  }
  return 0;    // Успех
}

// Адрес начала блока переменных uint16_t
#define VARS_START_ADDR (MAX_STRINGS * STRING_SIZE)    // 3200


// Записывает строку длиной до 15 символов + терминатор в EEPROM по номеру строки (0..MAX_STRINGS-1)
// string_num Номер строки (0..MAX_STRINGS-1)
// str Строка для записи
// ret 0 — успех, -1 — ошибка записи, -2 — номер строки вне диапазона
int eeprom_write_string_by_num(uint16_t string_num, const char *str) {
  if (string_num >= MAX_STRINGS) return -2;

  uint16_t addr = string_num * STRING_SIZE;
  uint16_t i    = 0;

  // Записываем до 15 символов, последний байт — терминатор
  while (i < STRING_SIZE - 1 && str[i] != '\0') {
    if (eeprom_write_byte(addr + i, (uint8_t)str[i]) != 0) {
      return -1;
    }
    i++;
  }
  // Записываем терминатор '\0'
  if (eeprom_write_byte(addr + i, '\0') != 0) { return -1; }

  // Остальные байты (если есть) заполняем нулями
  while (i < STRING_SIZE) {
    if (eeprom_write_byte(addr + i, '\0') != 0) {
      return -1;
    }
    i++;
  }
  return 0;
}

// При чтении каждого байта из EEPROM проверяется, равен ли он 0xFF
// Если да - заменяется на пробел ' ' (код 0x20)
// Если нет - используется как есть
int eeprom_read_string_by_num(uint16_t string_num, char *str) {
  if (str == NULL) return -1;    // Проверка указателя
  if (string_num >= MAX_STRINGS) return -2;    // Проверка диапазона

  uint16_t addr = string_num * STRING_SIZE;
  uint8_t byte;
  uint16_t i = 0;

  for (; i < STRING_SIZE - 1; i++) {    // Оставляем место для терминатора
    if (eeprom_read_byte(addr + i, &byte) != 0) {
      str[i] = '\0';                    // Завершаем строку при ошибке чтения
      return -1;                        // Возвращаем ошибку
    }

    // ЗАМЕНА 0xFF на пробел
    if (byte == 0xFF) {
      str[i] = ' ';
    } else {
      str[i] = (char)byte;
    }

    if (byte == '\0')
      break;    // Нашли конец строки
  }
  // Если не нашли '\0' - принудительно ставим в конце
  if (i == STRING_SIZE - 1) {
    str[i] = '\0';
  }
  return 0;
}


// Очищает строку в EEPROM (записывает все байты как 0xFF)
// string_num Номер строки (0..MAX_STRINGS-1)
// ret 0 — успех, -1 — ошибка записи, -2 — номер строки вне диапазона
int eeprom_clear_string(uint16_t string_num) {
  if (string_num >= MAX_STRINGS) return -2;

  uint16_t addr = string_num * STRING_SIZE;

  for (uint16_t i = 0; i < STRING_SIZE; i++) {
    if (eeprom_write_byte(addr + i, 0xFF) != 0) {
      return -1;
    }
  }
  return 0;
}


// Проверяет, есть ли данные в строке (не все байты 0xFF)
// string_num Номер строки (0..MAX_STRINGS-1)
// ret 1 — данные есть, 0 — пусто, -1 — ошибка чтения, -2 — номер строки вне диапазона
int eeprom_is_string_used(uint16_t string_num) {
  if (string_num >= MAX_STRINGS) return -2;

  uint16_t addr = string_num * STRING_SIZE;
  uint8_t byte;

  for (uint16_t i = 0; i < STRING_SIZE; i++) {
    if (eeprom_read_byte(addr + i, &byte) != 0)
      return -1;
    if (byte != 0xFF)
      return 1;    // Найден непустой байт
  }
  return 0;        // Все байты пустые (0xFF)
}


// Полностью очищает все строки (записывает 0xFF)
// ret 0 — успех, -1 — ошибка записи
int eeprom_clear_all_strings(void) {
  for (uint16_t i = 0; i < MAX_STRINGS; i++) {
    if (eeprom_clear_string(i) != 0)
      return -1;
  }
  return 0;
}


// Записывает uint16_t переменную в EEPROM по номеру (1..20)
// var_num Номер переменной (1..20)
// value Значение для записи
// ret 0 — успех, -1 — ошибка записи, -2 — номер вне диапазона
int eeprom_write_uint16_by_num(uint16_t var_num, uint16_t value) {
  if (var_num == 0 || var_num > VAR_UINT16_COUNT) return -2;

  uint16_t addr = VARS_START_ADDR + (var_num - 1) * 2;

  // Записываем младший байт
  if (eeprom_write_byte(addr, (uint8_t)(value & 0xFF)) != 0) return -1;
  // Записываем старший байт
  if (eeprom_write_byte(addr + 1, (uint8_t)((value >> 8) & 0xFF)) != 0) return -1;

  return 0;
}


// Читает uint16_t переменную из EEPROM по номеру (1..20)
// var_num Номер переменной (1..20)
// value Указатель для сохранения результата
// ret 0 — успех, -1 — ошибка чтения, -2 — номер вне диапазона
int eeprom_read_uint16_by_num(uint16_t var_num, uint16_t *value) {
  if (var_num == 0 || var_num > VAR_UINT16_COUNT) return -2;

  uint16_t addr = VARS_START_ADDR + (var_num - 1) * 2;
  uint8_t low, high;

  if (eeprom_read_byte(addr, &low) != 0) return -1;
  if (eeprom_read_byte(addr + 1, &high) != 0) return -1;

  *value = ((uint16_t)high << 8) | low;
  return 0;
}


// Очищает все 20 переменных (записывает 0xFFFF)
// ret 0 — успех, -1 — ошибка записи
int eeprom_clear_all_uint16_vars(void) {
  for (uint16_t i = 1; i <= VAR_UINT16_COUNT; i++) {
    uint16_t addr = VARS_START_ADDR + (i - 1) * 2;
    if (eeprom_write_byte(addr, 0xFF) != 0) return -1;
    if (eeprom_write_byte(addr + 1, 0xFF) != 0) return -1;
  }
  return 0;
}

// Eof eeprom.c