#include "rs485.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define RS485_DEFAULT_BAUD 4800

// Функция настройки UART1 на выбранную скорость (8N1)
void rs485_init(uint32_t baudrate) {
  // Включаем тактирование GPIOA, GPIOB, AFIO и USART1
  RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN | RCC_APB2ENR_AFIOEN | RCC_APB2ENR_USART1EN;

  // Настройка PA9 (TX) - AF push-pull 50MHz
  GPIOA->CRH &= ~(GPIO_CRH_MODE9 | GPIO_CRH_CNF9);
  GPIOA->CRH |= GPIO_CRH_MODE9_1 | GPIO_CRH_MODE9_0;    // MODE9 = 11 (50 MHz)
  GPIOA->CRH |= GPIO_CRH_CNF9_1;                        // CNF9 = 10 (AF push-pull)
  GPIOA->CRH &= ~GPIO_CRH_CNF9_0;

  // Настройка PA10 (RX) - Floating input
  GPIOA->CRH &= ~(GPIO_CRH_MODE10 | GPIO_CRH_CNF10);
  GPIOA->CRH |= GPIO_CRH_CNF10_0;    // CNF10 = 01 (floating input)
  GPIOA->CRH &= ~GPIO_CRH_CNF10_1;

  // Настройка PB15 (DE) - Output push-pull 50 MHz
  GPIOB->CRH &= ~(GPIO_CRH_MODE15 | GPIO_CRH_CNF15);
  GPIOB->CRH |= GPIO_CRH_MODE15_1 | GPIO_CRH_MODE15_0;    // MODE15 = 11 (50 MHz)
  GPIOB->CRH &= ~GPIO_CRH_CNF15;                          // CNF15 = 00 (push-pull)

  RS485DE0;                                               // Ставим DE = 0, приём по умолчанию

  // Настройка USART1
  USART1->CR1 = 0;    // сброс CR1

  // Вычисление значения BRR для заданной скорости
  // Частота USART1 (APB2) = 72 MHz
  uint32_t brr_value = 0;
  switch (baudrate) {
  case 4800:
    brr_value = 15000;    // 72_000_000 / 4800 = 15000
    break;
  case 9600:
    brr_value = 7500;     // 72_000_000 / 9600 = 7500
    break;
  case 19200:
    brr_value = 3750;     // 72_000_000 / 19200 = 3750
    break;
  case 38400:
    brr_value = 1875;     // 72_000_000 / 38400 = 1875
    break;
  case 115200:
    brr_value = 625;      // 72_000_000 / 115200 = 625
    break;
  default:
    brr_value = 15000;    // по умолчанию 4800 бод
    break;
  }
  USART1->BRR = brr_value;

  // Включаем передатчик и приёмник
  USART1->CR1 |= USART_CR1_TE | USART_CR1_RE;

  // Включаем USART1
  USART1->CR1 |= USART_CR1_UE;
}

// Функция передачи данных по RS485
void rs485_send(char *data, uint16_t len) {
  RS485DE1;    // Включаем передачу (DE = 1)

  for (uint16_t i = 0; i < len; i++) {
    while (!(USART1->SR & USART_SR_TXE))
      ;    // Ждем готовности передатчика
    USART1->DR = (data[i] & 0xFF);
  }

  while (!(USART1->SR & USART_SR_TC))
    ;          // Ждем окончания передачи последнего байта

  RS485DE0;    // Возвращаемся в режим приёма (DE = 0)
}

void rs485_init1(void) {
  // Включить тактирование USART1
  //  RCC->APB2ENR |= RCC_APB2ENR_USART1EN;

  // Настройка PA9 (TX) — альтернативная функция push-pull, 50 MHz
  GPIOA->CRH &= ~(GPIO_CRH_MODE9 | GPIO_CRH_CNF9);
  GPIOA->CRH |= GPIO_CRH_MODE9_1 | GPIO_CRH_MODE9_0;    // 50 MHz
  GPIOA->CRH |= GPIO_CRH_CNF9_1;                        // AF push-pull
  GPIOA->CRH &= ~GPIO_CRH_CNF9_0;

  // Настройка PA10 (RX) — вход, плавающий
  GPIOA->CRH &= ~(GPIO_CRH_MODE10 | GPIO_CRH_CNF10);
  GPIOA->CRH |= GPIO_CRH_CNF10_0;    // Floating input
  GPIOA->CRH &= ~GPIO_CRH_CNF10_1;

  // USART1 настройка: 4800 бод, 8N1
  USART1->CR1 = 0;                               // сброс
  USART1->BRR = 15000;                           // 72MHz / 4800 = 15000
  USART1->CR1 |= USART_CR1_TE | USART_CR1_RE;    // TX и RX включены
  USART1->CR1 |= USART_CR1_UE;                   // USART включен
}

void rs485_send_string_with_se_markers(const char *str) {
  // Выделим буфер длиной len + 2 (для < и >)
  uint16_t len = 0;
  while (str[len] && len < TRANSMITLENGHT - 2)
    len++;

  char buffer[TRANSMITLENGHT] = {0};
  buffer[0]                   = '<';
  for (uint16_t i = 0; i < len; i++) {
    buffer[i + 1] = str[i];
  }
  buffer[len + 1] = '>';

  rs485_send(buffer, len + 2);
}

void rs485_send_string_with_params(char brightness, char res1, char res2, const char *str) {
  // Убедимся, что brightness — цифра от 0 до 9
  if (brightness < 0)
    brightness = 0;
  if (brightness > 9)
    brightness = 9;

  // Рассчитываем максимальную длину строки для копирования
  uint16_t max_payload_len = TRANSMITLENGHT - 2 - 3;    // 2 — для '<' и '>', 3 — для добавленных символов
  uint16_t len             = 0;
  while (str[len] && len < max_payload_len)
    len++;

  char buffer[TRANSMITLENGHT] = {0};
  buffer[0]                   = '<';

  // Вставляем символы яркости и резервные символы
  buffer[1] = '0' + brightness;
  buffer[2] = res1;
  buffer[3] = res2;

  // Копируем исходную строку после первых 3 символов
  for (uint16_t i = 0; i < len; i++) {
    buffer[i + 4] = str[i];
  }

  buffer[len + 4] = '>';

  // Отправляем буфер длиной: 2 (рамки) + 3 (новые символы) + len (строка)
  rs485_send(buffer, len + 5);
}

// Возвращает количество байт записанных в utf8_buf (1..2)
int cp1251_to_utf8(uint8_t cp1251_char, char *utf8_buf) {
  // Таблица соответствия для кодов CP1251 0x80..0xFF в Unicode
  static const uint16_t cp1251_to_unicode[128] = {
      0x0402, 0x0403, 0x201A, 0x0453, 0x201E, 0x2026, 0x2020, 0x2021, 0x20AC, 0x2030, 0x0409, 0x2039, 0x040A,
      0x040C, 0x040B, 0x040F, 0x0452, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x0000, 0x2122,
      0x0459, 0x203A, 0x045A, 0x045C, 0x045B, 0x045F, 0x00A0, 0x040E, 0x045E, 0x0408, 0x00A4, 0x0490, 0x00A6,
      0x00A7, 0x0401, 0x00A9, 0x0404, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x0407, 0x00B0, 0x00B1, 0x0406, 0x0456,
      0x0491, 0x00B5, 0x00B6, 0x00B7, 0x0451, 0x2116, 0x0454, 0x00BB, 0x0458, 0x0405, 0x0455, 0x0457, 0x0410,
      0x0411, 0x0412, 0x0413, 0x0414, 0x0415, 0x0416, 0x0417, 0x0418, 0x0419, 0x041A, 0x041B, 0x041C, 0x041D,
      0x041E, 0x041F, 0x0420, 0x0421, 0x0422, 0x0423, 0x0424, 0x0425, 0x0426, 0x0427, 0x0428, 0x0429, 0x042A,
      0x042B, 0x042C, 0x042D, 0x042E, 0x042F, 0x0430, 0x0431, 0x0432, 0x0433, 0x0434, 0x0435, 0x0436, 0x0437,
      0x0438, 0x0439, 0x043A, 0x043B, 0x043C, 0x043D, 0x043E, 0x043F, 0x0440, 0x0441, 0x0442, 0x0443, 0x0444,
      0x0445, 0x0446, 0x0447, 0x0448, 0x0449, 0x044A, 0x044B, 0x044C, 0x044D, 0x044E, 0x044F};

  uint16_t unicode_char;

  if (cp1251_char < 0x80) {
    // ASCII символ — совпадает с Unicode
    unicode_char = cp1251_char;
  } else {
    // CP1251 с 0x80 по 0xFF
    unicode_char = cp1251_to_unicode[cp1251_char - 0x80];
    if (unicode_char == 0) {
      // Символ отсутствует в таблице — заменяем на '?'
      unicode_char = '?';
    }
  }

  if (unicode_char < 0x80) {
    // 1 байт UTF-8 (ASCII)
    utf8_buf[0] = (char)unicode_char;
    return 1;
  } else {
    // 2 байта UTF-8
    utf8_buf[0] = (char)(0xC0 | ((unicode_char >> 6) & 0x1F));
    utf8_buf[1] = (char)(0x80 | (unicode_char & 0x3F));
    return 2;
  }
}

// Возвращает выделенную в куче UTF-8 строку или NULL при ошибке
char *cp1251_to_utf8_alloc(const char *cp1251_str) {
  if (!cp1251_str)
    return NULL;

  // Оценка максимального размера: каждый символ CP1251 может стать до 2 байт UTF-8
  size_t max_len = 2 * strlen(cp1251_str) + 1;
  char *utf8_str = malloc(max_len);
  if (!utf8_str)
    return NULL;

  size_t i = 0, j = 0;
  while (cp1251_str[i]) {
    char tmp[2];
    int len = cp1251_to_utf8((uint8_t)cp1251_str[i], tmp);
    if (j + len >= max_len) {
      // Теоретически не должно случиться из-за выделенного размера,
      // но проверка на всякий случай
      break;
    }
    for (int k = 0; k < len; k++) {
      utf8_str[j++] = tmp[k];
    }
    i++;
  }
  utf8_str[j] = '\0';
  return utf8_str;
}

// Eof rs485.c