#ifndef __RS485_H__
#define __RS485_H__
#include "stm32f10x.h"

#define RS485DE1 GPIOB->BSRR = GPIO_BSRR_BS15  // DE = 1 (передача)
#define RS485DE0 GPIOB->BSRR = GPIO_BSRR_BR15  // DE = 0 (приём)
#define RS485_DEFAULT_BAUD 4800
#define TRANSMITLENGHT 28

void rs485_init1(void);
void rs485_init(uint32_t baudrate);
void rs485_send(char *data, uint16_t len);
void rs485_send_string_with_se_markers(const char *str);
void rs485_send_string_with_params(char brightness, char res1, char res2, const char *str);
// Возвращает количество байт записанных в utf8_buf (1..2)
int cp1251_to_utf8(uint8_t cp1251_char, char *utf8_buf);
// Возвращает выделенную в куче UTF-8 строку или NULL при ошибке
char *cp1251_to_utf8_alloc(const char *cp1251_str);

#endif // __RS485_H__