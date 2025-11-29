#include "stm32f10x.h"
#include "eeprom.h"

#define EEPROM_SIZE 4096
#define STRING_SIZE 32        // Изменено с 16 на 32
#define MAX_STRINGS 100       // Изменено с 200 на 100
#define VAR_UINT16_COUNT 20
#define EEPROM_PAGE_SIZE 32
#define WAITCNT 5000

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
  uint32_t timeout = WAITCNT;
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
  delay_simple(WAITCNT);
  return 0;
}

// Запись страницы в EEPROM (последовательная запись нескольких байт)
int eeprom_write_page(uint16_t addr, uint8_t *data, uint16_t len) {
  if (len == 0) return 0;
  
  // Проверяем, чтобы запись не выходила за границу страницы
  uint16_t page_boundary = (addr / EEPROM_PAGE_SIZE + 1) * EEPROM_PAGE_SIZE;
  if (addr + len > page_boundary) {
    len = page_boundary - addr;  // Ограничиваем длину записи
  }

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
  for (uint16_t i = 0; i < len; i++) {
    I2C1->DR = data[i];
    if (!I2C_WaitEvent(I2C_SR1_TXE)) return -1;
  }

  I2C1->CR1 |= I2C_CR1_STOP;
  // Задержка для записи страницы (может быть больше чем для одного байта)
  delay_simple(WAITCNT);
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

// Чтение страницы из EEPROM (последовательное чтение нескольких байт)
int eeprom_read_page(uint16_t addr, uint8_t *data, uint16_t len) {
  if (len == 0) return 0;

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

  // Читаем несколько байт последовательно
  for (uint16_t i = 0; i < len; i++) {
    if (i == len - 1) {
      // Для последнего байта отправляем NACK
      I2C1->CR1 &= ~I2C_CR1_ACK;
    } else {
      // Для всех кроме последнего отправляем ACK
      I2C1->CR1 |= I2C_CR1_ACK;
    }

    if (!I2C_WaitEvent(I2C_SR1_RXNE)) return -1;
    data[i] = I2C1->DR;
  }

  I2C1->CR1 |= I2C_CR1_STOP;
  I2C1->CR1 |= I2C_CR1_ACK;  // Восстанавливаем ACK для следующих операций

  return 0;
}

// Записывает строку в EEPROM (постраничная запись)
// addr Начальный адрес для записи
// str Указатель на строку (максимум 31 символ + терминатор)
// ret 0 в случае успеха, -1 при ошибке
int eeprom_write_string(uint16_t addr, const char *str) {
  uint8_t buffer[STRING_SIZE] = {0};
  uint16_t i = 0;

  // Копируем строку в буфер
  while (i < STRING_SIZE - 1 && str[i] != '\0') {
    buffer[i] = (uint8_t)str[i];
    i++;
  }
  
  // Гарантируем нулевой терминатор
  buffer[i] = '\0';

  // Записываем всю строку одной страницей
  return eeprom_write_page(addr, buffer, STRING_SIZE);
}

// Читает строку из EEPROM (постраничное чтение)
// addr Начальный адрес для чтения
// str Буфер для строки (минимум 32 байт)
// ret 0 в случае успеха, -1 при ошибке
int eeprom_read_string(uint16_t addr, char *str) {
  uint8_t buffer[STRING_SIZE];

  // Читаем всю строку одной страницей
  if (eeprom_read_page(addr, buffer, STRING_SIZE) != 0) {
    return -1;    // Ошибка чтения
  }

  // Копируем данные в буфер строки
  for (uint16_t i = 0; i < STRING_SIZE; i++) {
    str[i] = (char)buffer[i];
    if (buffer[i] == '\0') {
      break;    // Встретили терминатор - выходим
    }
  }

  // Гарантируем, что строка всегда заканчивается терминатором
  str[STRING_SIZE - 1] = '\0';
  return 0;    // Успех
}

// Адрес начала блока переменных uint16_t
#define VARS_START_ADDR (MAX_STRINGS * STRING_SIZE)    // 100 * 32 = 3200

// Записывает строку длиной до 31 символов + терминатор в EEPROM по номеру строки (0..MAX_STRINGS-1)
// string_num Номер строки (0..MAX_STRINGS-1)
// str Строка для записи
// ret 0 — успех, -1 — ошибка записи, -2 — номер строки вне диапазона
int eeprom_write_string_by_num(uint16_t num, const char *data)
{
    if (!data) return -1;  // вместо ERR_PARAM

    uint16_t addr = num * STRING_SIZE;
    uint8_t buf[STRING_SIZE];

    for (int i = 0; i < STRING_SIZE; i++) {
        buf[i] = data[i];  // копируем все байты, включая нули
    }

    return eeprom_write_page(addr, buf, STRING_SIZE);
}
// Чтение строки по номеру с постраничным чтением
// string_num Номер строки (0..MAX_STRINGS-1)
// str Буфер для строки (минимум STRING_SIZE байт)
// ret 0 — успех, -1 — ошибка чтения, -2 — номер строки вне диапазона
int eeprom_read_string_by_num(uint16_t string_num, char *str)
{
    if (!str) return -1;                    // проверка на NULL
    if (string_num >= MAX_STRINGS) return -2; // проверка выхода за границы

    uint16_t addr = string_num * STRING_SIZE;
    uint8_t buffer[STRING_SIZE];

    if (eeprom_read_page(addr, buffer, STRING_SIZE) != 0) {
        // при ошибке чтения возвращаем пустую строку (только для main.c)
        for (int i = 0; i < STRING_SIZE; i++) str[i] = ' ';
        str[STRING_SIZE] = '\0';
        return -1;
    }

    // Копируем байты как есть
    for (int i = 0; i < STRING_SIZE; i++) str[i] = buffer[i];

    // Терминатор добавляем только для работы в main.c
    str[STRING_SIZE] = '\0';

    return 0;
}

// Очищает строку в EEPROM (записывает все байты как 0xFF)
// string_num Номер строки (0..MAX_STRINGS-1)
// ret 0 — успех, -1 — ошибка записи, -2 — номер строки вне диапазона
int eeprom_clear_string(uint16_t string_num) {
  if (string_num >= MAX_STRINGS) return -2;

  uint16_t addr = string_num * STRING_SIZE;
  
  // Очищаем побайтово - это надежнее для EEPROM
  for (uint16_t i = 0; i < STRING_SIZE; i++) {
    if (eeprom_write_byte(addr + i, 0xFF) != 0) {
      return -1;
    }
    // Небольшая задержка между байтами
    delay_simple(1000);
  }
  
  return 0;
}

// Проверяет, есть ли данные в строке (не все байты 0xFF)
// string_num Номер строки (0..MAX_STRINGS-1)
// ret 1 — данные есть, 0 — пусто, -1 — ошибка чтения, -2 — номер строки вне диапазона
int eeprom_is_string_used(uint16_t string_num) {
  if (string_num >= MAX_STRINGS) return -2;

  uint16_t addr = string_num * STRING_SIZE;
  uint8_t buffer[STRING_SIZE];

  // Используем постраничное чтение для проверки
  if (eeprom_read_page(addr, buffer, STRING_SIZE) != 0)
    return -1;

  for (uint16_t i = 0; i < STRING_SIZE; i++) {
    if (buffer[i] != 0xFF)
      return 1;    // Найден непустой байт
  }
  return 0;        // Все байты пустые (0xFF)
}

// Полностью очищает все строки (записывает 0xFF)
// ret 0 — успех, -1 — ошибка записи
int eeprom_clear_all_strings(void) {
  // Очищаем каждую строку отдельно с задержками
  for (uint16_t i = 0; i < MAX_STRINGS; i++) {
    if (eeprom_clear_string(i) != 0) {
      return -1;
    }
    // Задержка между строками
    delay_simple(5000);
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
  uint8_t buffer[2] = {(uint8_t)(value & 0xFF), (uint8_t)((value >> 8) & 0xFF)};

  // Записываем оба байта одной страницей
  return eeprom_write_page(addr, buffer, 2);
}

// Читает uint16_t переменную из EEPROM по номеру (1..20)
// var_num Номер переменной (1..20)
// value Указатель для сохранения результата
// ret 0 — успех, -1 — ошибка чтения, -2 — номер вне диапазона
int eeprom_read_uint16_by_num(uint16_t var_num, uint16_t *value) {
  if (var_num == 0 || var_num > VAR_UINT16_COUNT) return -2;

  uint16_t addr = VARS_START_ADDR + (var_num - 1) * 2;
  uint8_t buffer[2];

  // Читаем оба байта одной страницей
  if (eeprom_read_page(addr, buffer, 2) != 0) return -1;

  *value = ((uint16_t)buffer[1] << 8) | buffer[0];
  return 0;
}

// Очищает все 20 переменных (записывает 0xFFFF)
// ret 0 — успех, -1 — ошибка записи
int eeprom_clear_all_uint16_vars(void) {
  // Очищаем каждую переменную отдельно
  for (uint16_t i = 1; i <= VAR_UINT16_COUNT; i++) {
    uint16_t addr = VARS_START_ADDR + (i - 1) * 2;
    
    // Записываем 0xFF в каждый байт
    if (eeprom_write_byte(addr, 0xFF) != 0) return -1;
    delay_simple(1000);
    
    if (eeprom_write_byte(addr + 1, 0xFF) != 0) return -1;
    delay_simple(1000);
  }
  return 0;
}

#include  "stdlib.h"
#define		_EEPROM_SIZE_KBIT		32       /* 256K (32,768 x 8) */
#define		_EEPROM_ADDRESS			0xA0
#if (_EEPROM_SIZE_KBIT == 1) || (_EEPROM_SIZE_KBIT == 2)
#define _EEPROM_PSIZE     8
#elif (_EEPROM_SIZE_KBIT == 4) || (_EEPROM_SIZE_KBIT == 8) || (_EEPROM_SIZE_KBIT == 16)
#define _EEPROM_PSIZE     16
#else
#define _EEPROM_PSIZE     32
#endif

/**
  * @brief  Checks if memory device is ready for communication.
  * @param  none
  * @retval bool
  */
uint8_t at24_isConnected(void);
uint8_t at24_write(uint16_t address, uint8_t *data, size_t lenInBytes, uint32_t timeout);
uint8_t at24_eraseChip(void)
{
  const uint8_t eraseData[32] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF\
    , 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  uint32_t bytes = 0;
  while ( bytes < (_EEPROM_SIZE_KBIT * 128))
  {
    if (at24_write(bytes, (uint8_t*)eraseData, sizeof(eraseData), 100) != 0)
      return -1;
    bytes += sizeof(eraseData);           
  }
  return 0;  
}

// Eof eeprom.c
