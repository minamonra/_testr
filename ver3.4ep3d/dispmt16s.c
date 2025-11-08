#include "dispmt16s.h"
#define SDELAY 36     // ~500ns (вместо 100)
#define LDELAY 720    // ~10μs (вместо 1000)

void delay_nop(uint32_t count) {
  for (volatile uint32_t i = 0; i < count; i++) {
    __NOP();
  }
}

// === Надежная и быстрая конвертация UTF-8 -> CP1251 ===
char safe_utf8_to_cp1251(const char **src) {
  const unsigned char *s = (const unsigned char *)*src;

  // ASCII (0x00-0x7F)
  if (s[0] < 0x80) {
    (*src)++;
    return s[0];
  }

  // Проверяем что есть второй байт
  if (s[1] == 0) {
    (*src)++;
    return '?';
  }

  // Русские буквы А-Я, а-я
  if (s[0] == 0xD0) {
    if (s[1] >= 0x90 && s[1] <= 0xBF) {
      (*src) += 2;
      return s[1] + 0x30;    // А-п
    }
    if (s[1] == 0x81) {
      (*src) += 2;
      return 0xA8;    // Ё
    }
  } else if (s[0] == 0xD1) {
    if (s[1] >= 0x80 && s[1] <= 0x8F) {
      (*src) += 2;
      return s[1] + 0x70;    // р-я
    }
    if (s[1] == 0x91) {
      (*src) += 2;
      return 0xB8;    // ё
    }
  }

  // Неподдерживаемый UTF-8 символ
  (*src)++;
  return '?';
}

// === Отправка 4-битного ниббла ===
void lcdNibble(uint8_t nibble) {
  // Устанавливаем линии данных (D7..D4)
  if (nibble & 0x08)
    D71;
  else
    D70;
  if (nibble & 0x04)
    D61;
  else
    D60;
  if (nibble & 0x02)
    D51;
  else
    D50;
  if (nibble & 0x01)
    D41;
  else
    D40;

  // Импульс тактирования (Enable)
  EN1;
  delay_nop(SDELAY);
  EN0;
  delay_nop(SDELAY);
}

// === Отправка байта (команда или данные) ===
void lcdSend(uint8_t isCommand, uint8_t data) {
  if (isCommand)
    RS0;
  else
    RS1;
  lcdNibble(data >> 4);      // старший ниббл
  lcdNibble(data & 0x0F);    // младший ниббл
  delay_nop(LDELAY);         // стандартная задержка после команды/данных
}

// === Отправка команды ===
void lcdCommand(uint8_t cmd) {
  lcdSend(1, cmd);
}

// === Вывод символа ===
void lcdChar(char chr) {
  lcdSend(0, chr);
}

// === Вывод строки с указанием линии ===
// line = 0 -> первая строка, line = 1 -> вторая строка
void lcdString(const char *s, uint8_t line) {
  uint8_t addr = (line == 0) ? 0x00 : 0x40;    // адрес начала строки
  lcdCommand(0x80 | addr);                     // установка курсора
  while (*s) {
    lcdChar(*s++);
  }
}

// === Очистка экрана с задержкой ===
void lcdClear(void) {
  lcdCommand(0x01);         // команда очистки экрана
  delay_nop(LDELAY * 4);
  lcdCommand(0x01);
  delay_nop(LDELAY * 4);    // увеличенная задержка после очистки
}

// === Установка курсора на позицию col, line (0 или 1) ===
void lcdSetCursor(uint8_t col, uint8_t line) {
  uint8_t addr = (line == 0) ? 0x00 : 0x40;
  addr += col;
  lcdCommand(0x80 | addr);
}

// === Вывод символа в позицию с управлением миганием курсора ===
// pos = 0..15 (на строке), line = 0/1
// blink = 0 -> курсор и мигание выключены, 1 -> включены
void lcdCharAt(char chr, uint8_t line, uint8_t pos, uint8_t blink) {
  uint8_t addr = (line == 0) ? 0x00 : 0x40;
  lcdCommand(0x80 | (addr + pos));    // установка курсора в позицию

  if (blink) {
    lcdCommand(0x0D);                 // дисплей вкл, курсор вкл, мигание вкл
  } else {
    lcdCommand(0x0C);                 // дисплей вкл, курсор выкл, мигание выкл
  }

  lcdSend(0, chr);                    // вывод символа
}

// === Управление курсором с опцией включения/выключения ===
void lcdSetCursorB(uint8_t col, uint8_t line, char cursor_enabled) {
  uint8_t addr = (line == 0) ? 0x00 : 0x40;
  addr += col;

  lcdCommand(0x80 | addr);

  if (cursor_enabled) {
    lcdCommand(0x0E);    // включение курсора без мигания
    // lcdCommand(0x0F);  // включение курсора с миганием (по желанию)
  } else {
    lcdCommand(0x0C);    // выключение курсора
  }
}

#define NO_CURSOR 0        // курсор выключен
#define NORMAL_CURSOR 1    // обычный курсор (не мигающий)
#define BLINK_CURSOR 2     // мигающий курсор

// === Установка курсора с режимом отображения курсора ===
void lcdSetCursorN(uint8_t col, uint8_t line, char cursor_mode) {
  uint8_t addr = (line == 0) ? 0x00 : 0x40;
  addr += col;

  lcdCommand(0x80 | addr);

  if (cursor_mode == NO_CURSOR) {
    lcdCommand(0x0C);    // курсор выключен
  } else if (cursor_mode == NORMAL_CURSOR) {
    lcdCommand(0x0E);    // обычный курсор
  } else if (cursor_mode == BLINK_CURSOR) {
    lcdCommand(0x0F);    // мигающий курсор
  }
}

// === Вывод строки с указанием позиции курсора ===
void lcdPrintAt(const char *s, uint8_t col, uint8_t line) {
  lcdSetCursor(col, line);
  while (*s) {
    lcdChar(*s++);
  }
}

// === Инициализация дисплея ===
void lcd_init1(void) {
  delay_nop(20000);   // ждём >15 мс после подачи питания
  RS0;  // RS = 0 для команд
  // Инициализация 8-битного режима (три импульса EN)
  for (int i = 0; i < 3; i++) {
    EN1;
    delay_nop(SDELAY);
    EN0;
    delay_nop(LDELAY);
  }
  // Переход в 4-битный режим — отправляем 0x02 (D7..D4 = 0010)
  D70; D60; D51; D40;  // выставляем 0x02 на линии данных
  EN1;
  delay_nop(SDELAY);
  EN0;
  delay_nop(LDELAY);
  lcdCommand(0x28);  // Без этого почему-то никак
  lcdCommand(0x2A);  // Функциональная установка: 4-бит, 2 линии, 5x8 точек (0x28 или 0x2A зависит от контроллера)
  lcdCommand(0x0C);  // Включаем дисплей, курсор выключен
  lcdClear();        // Очистка экрана с задержкой
  lcdCommand(0x06);  // Режим ввода: курсор сдвигается вправо
}

void lcd_init(void) {
  delay_nop(20000);   // ждём >15 мс после подачи питания

  RS0;  // RS = 0 для команд

  // Инициализация 8-битного режима (три импульса EN)
  for (int i = 0; i < 3; i++) {
    EN1;
    delay_nop(SDELAY);
    EN0;
    delay_nop(LDELAY);
  }

  // Переход в 4-битный режим — отправляем 0x02 (D7..D4 = 0010)
  D70; D60; D51; D40;  // выставляем 0x02 на линии данных
  EN1;
  delay_nop(SDELAY);
  EN0;
  delay_nop(LDELAY);

  lcdCommand(0x2A);  // Функциональная установка: 4-бит, 2 линии, 5x8 точек (0x28 или 0x2A зависит от контроллера)
  lcdCommand(0x0C);  // Включаем дисплей, курсор выключен
  lcdClear();        // Очистка экрана с задержкой
  lcdCommand(0x06);  // Режим ввода: курсор сдвигается вправо
}


// === Быстрая перекодировка UTF-8 -> CP1251 (кириллица) ===
char utf8_to_cp1251_char(const char **src) {
  unsigned char c = (unsigned char)(*src)[0];

  if (c < 0x80) {
    (*src)++;
    return c;
  }

  unsigned char c2 = (unsigned char)(*src)[1];

  if (c == 0xD0) {
    (*src) += 2;
    if (c2 >= 0x90 && c2 <= 0xBF)
      return c2 + 0x30;    // А–п
    else if (c2 == 0x81)
      return 0xA8;         // Ё
  } else if (c == 0xD1) {
    (*src) += 2;
    if (c2 >= 0x80 && c2 <= 0x8F)
      return c2 + 0x70;    // р–я
    else if (c2 == 0x91)
      return 0xB8;         // ё
  }

  (*src)++;
  return '?';
}

// === Вывод UTF-8 строки (кириллица) ===
void lcdPrintUtf8(const char *s, uint8_t line) {
  // lcdSetCursor(0, line);
  while (*s) {
    char ch = utf8_to_cp1251_char(&s);
    lcdChar(ch);
  }
}

// === Вывод строки длиной до 16 символов с заполнением пробелами ===
void lcdString16(const char *s, uint8_t line) {
  uint8_t addr = (line == 0) ? 0x00 : 0x40;
  lcdCommand(0x80 | addr);

  uint8_t count = 0;
  while (s[count] && count < 16) {
    lcdChar(s[count++]);
  }
  while (count < 16) {
    lcdChar(' ');
    count++;
  }
}

void lcdString16_with_brackets(const char *s, uint8_t line) {
  char buffer[17] = {0};    // 16 символов + 1 нуль

  buffer[0] = '[';

  // Копируем 12 символов из s (с 1-го индекса buffer), заменяем \0 на пробел
  for (int i = 0; i < 12; i++) {
    if (s[i] == '\0') {
      buffer[1 + i] = ' ';
    } else {
      buffer[1 + i] = s[i];
    }
  }

  buffer[13] = ']';
  buffer[14] = ' ';
  buffer[15] = ' ';
  buffer[16] = '\0';

  lcdString16(buffer, line);
}

// === Преобразование числа в строку (itoa) ===
char *my_itoa(int32_t num, char *str, uint8_t base) {
  int i          = 0;
  int isNegative = 0;

  if (num == 0) {
    str[i++] = '0';
    str[i]   = '\0';
    return str;
  }

  if (num < 0 && base == 10) {
    isNegative = 1;
    num        = -num;
  }

  while (num != 0) {
    int rem  = num % base;
    str[i++] = (rem > 9) ? (rem - 10) + 'A' : rem + '0';
    num      = num / base;
  }

  if (isNegative)
    str[i++] = '-';

  str[i] = '\0';

  // разворот строки
  int start = 0, end = i - 1;
  while (start < end) {
    char tmp   = str[start];
    str[start] = str[end];
    str[end]   = tmp;
    start++;
    end--;
  }

  return str;
}

// === Вывод двухзначного числа с ведущим нулём ===
void lcdPrintTwoDigitNumber(int num) {
  char buf[4] = {0};

  if (num < 0)
    num = 0;
  if (num > 99)
    num = 99;

  my_itoa(num, buf, 10);

  if (buf[1] == '\0') {
    lcdChar('0');
    lcdChar(buf[0]);
  } else {
    lcdChar(buf[0]);
    lcdChar(buf[1]);
  }
}

void lcdClearViaChars(void) {
  // Перемещаем курсор в начало первой строки
  lcdSetCursor(0, 0);
  for (int i = 0; i < 16; i++) {
    lcdChar(' ');
  }
  // Перемещаем курсор в начало второй строки
  lcdSetCursor(0, 1);
  for (int i = 0; i < 16; i++) {
    lcdChar(' ');
  }
  // Вернуть курсор в начало
  lcdSetCursor(0, 0);
}

// Функции-обертки для lcdSend
// void lcdCommand(uint8_t cmd) {
//    lcdSend(1, cmd); // 1 - это команда (RS0)
//}

void lcdData(uint8_t data) {
  lcdSend(0, data);    // 0 - это данные (RS1)
}

// Функция загрузки пользовательского символа в CGRAM
void lcdLoadCustomChar(uint8_t char_num, const uint8_t *pattern) {
  // Устанавливаем адрес в CGRAM (0x40 + номер символа * 8)
  lcdCommand(0x40 + (char_num * 8));

  // Отправляем все 8 байт паттерна
  for (uint8_t i = 0; i < 8; i++) {
    lcdData(pattern[i]);
  }

  // Возвращаемся в режим DDRAM
  lcdCommand(0x80);
}

//// Паттерны для анимации
// const uint8_t patterns[][8] = {
//     {0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00}, // Маленькая точка
//     {0x00, 0x00, 0x18, 0x3C, 0x3C, 0x18, 0x00, 0x00}, // Круг
//     {0x00, 0x3C, 0x7E, 0x7E, 0x7E, 0x7E, 0x3C, 0x00}, // Большой круг
//     {0x18, 0x3C, 0x7E, 0xFF, 0xFF, 0x7E, 0x3C, 0x18}, // Ромб
//     {0x00, 0x24, 0x5A, 0x24, 0x5A, 0x24, 0x00, 0x00}, // Шахматы
// };

// void show_screensaver(void) {
//     static uint8_t current_pattern = 0;
//     static uint8_t animation_frame = 0;
//     static uint8_t positions[2][16] = {0};

//    // Анимируем паттерны
//    if (animation_frame % 5 == 0) {
//        current_pattern = (current_pattern + 1) % (sizeof(patterns) / sizeof(patterns[0]));

//        // Загружаем новый паттерн в CGRAM
//        lcdLoadCustomChar(0, patterns[current_pattern]);

//        // Обновляем экран
//        for (uint8_t line = 0; line < 2; line++) {
//            for (uint8_t pos = 0; pos < 16; pos++) {
//                if (simple_rand() % 4 == 0 || positions[line][pos] == 0) {
//                    positions[line][pos] = simple_rand() % 2 ? 0 : 1;
//                    lcdSetCursor(pos, line);
//                    lcdChar(positions[line][pos] ? 0 : ' ');
//                }
//            }
//        }
//    }

//    animation_frame++;

//    // Периодически очищаем экран
//    if (animation_frame > 100) {
//        animation_frame = 0;
//        for (uint8_t line = 0; line < 2; line++) {
//            for (uint8_t pos = 0; pos < 16; pos++) {
//                positions[line][pos] = 0;
//            }
//        }
//        lcdClear();
//    }
//}

// void show_screensaver1(void) {
//     // Случайно добавляем новый символ
//     if (simple_rand() % 4 == 0) { // Реже появляются новые символы
//         uint8_t line = simple_rand() % 2;
//         uint8_t pos = simple_rand() % 16;
//         uint8_t char_index = simple_rand() % (sizeof(cp1251_chars) - 1);

//        current_chars[line][pos] = cp1251_chars[char_index];
//        fade_timer[line][pos] = 5; // Начальное значение таймера
//    }

//    // Обновляем экран
//    for (uint8_t line = 0; line < 2; line++) {
//        for (uint8_t pos = 0; pos < 16; pos++) {
//            if (fade_timer[line][pos] > 0) {
//                fade_timer[line][pos]--;

//                // Случайное мерцание при затухании
//                if (fade_timer[line][pos] > 0 || simple_rand() % 3 == 0) {
//                    lcdSetCursor(pos, line);
//                    lcdChar(current_chars[line][pos]);
//                } else {
//                    lcdSetCursor(pos, line);
//                    lcdChar(' '); // Полное затухание
//                }
//            }
//        }
//    }

//    // Изредка добавляем "капли" - целые колонки символов
//    if (simple_rand() % 50 == 0) {
//        uint8_t pos = simple_rand() % 16;
//        for (uint8_t line = 0; line < 2; line++) {
//            uint8_t char_index = simple_rand() % (sizeof(cp1251_chars) - 1);
//            current_chars[line][pos] = cp1251_chars[char_index];
//            fade_timer[line][pos] = 5;
//        }
//    }
//}
