#ifndef __DISPMT16S_H__
#define __DISPMT16S_H__
#include "stm32f10x.h"

// PA15-RS PB3-E PB4-D4 PB5-D5 PB8-D6 PB9-D7
// === Макросы для управления пинами дисплея ===
// RS = PA15
#define RS1 GPIOA->BSRR |= GPIO_BSRR_BS15    // set (1)
#define RS0 GPIOA->BSRR |= GPIO_BSRR_BR15    // reset (0)
// E (en) = PB3
#define EN1 GPIOB->BSRR |= GPIO_BSRR_BS3
#define EN0 GPIOB->BSRR |= GPIO_BSRR_BR3
// D4 = PB4
#define D41 GPIOB->BSRR |= GPIO_BSRR_BS4
#define D40 GPIOB->BSRR |= GPIO_BSRR_BR4
// D5 = PB5
#define D51 GPIOB->BSRR |= GPIO_BSRR_BS5
#define D50 GPIOB->BSRR |= GPIO_BSRR_BR5
// D6 = PB8
#define D61 GPIOB->BSRR |= GPIO_BSRR_BS8
#define D60 GPIOB->BSRR |= GPIO_BSRR_BR8
// D7 = PB9
#define D71 GPIOB->BSRR |= GPIO_BSRR_BS9
#define D70 GPIOB->BSRR |= GPIO_BSRR_BR9

void lcd_init(void);
void lcdCommand(uint8_t cmd);
void lcdStringС(const char *s);
void lcdString(const char *s, uint8_t line);
void lcdChar(char chr);
void lcdCharAt(char chr, uint8_t line, uint8_t pos, uint8_t blink);
void lcdSetCursor(uint8_t col, uint8_t line);
void lcdPrintAt(const char *s, uint8_t col, uint8_t line);
void lcdSetCursorB(uint8_t col, uint8_t line, char cursor_enabled);
void lcdSetCursorN(uint8_t col, uint8_t line, char cursor_mode);
char utf8_to_cp1251_char(const char **src);
void lcdClear(void);
void lcdPrintUtf8(const char *s, uint8_t line);
void lcdString16(const char *s, uint8_t line);
void lcdString16_with_brackets(const char *s, uint8_t line);
char *my_itoa(int32_t num, char *str, uint8_t base);
void lcdPrintTwoDigitNumber(int num);
void lcdClearViaChars(void);
void lcdData(uint8_t data);
void lcdLoadCustomChar(uint8_t char_num, const uint8_t *pattern);

#endif    // __DISPMT16S_H__