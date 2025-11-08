#pragma once
#ifndef __COMMON_H__
#define __COMMON_H__
#include "stm32f10x.h"
#include <stddef.h>

/************************* IWDG *************************/
#define IWDG_REFRESH      (uint32_t)(0x0000AAAA)
#define IWDG_WRITE_ACCESS (uint32_t)(0x00005555)
#define IWDG_START        (uint32_t)(0x0000CCCC)
void iwdg_setup(void);

extern const unsigned char cp1251_chars[];

#define LED1TOGGLE GPIOC->ODR ^= (1<<13)
#define LED2TOGGLE GPIOC->ODR ^= (1<<14)

// MODE:
#define MODE_INPUT      0
#define MODE_NORMAL     1  // 10MHz
#define MODE_SLOW       2  // 2MHz
#define MODE_FAST       3  // 50MHz
// CNF:
#define CNF_ANALOG      (0<<2)
#define CNF_PPOUTPUT    (0<<2)
#define CNF_FLINPUT     (1<<2)
#define CNF_ODOUTPUT    (1<<2)
#define CNF_PUDINPUT    (2<<2)
#define CNF_AFPP        (2<<2)
#define CNF_AFOD        (3<<2)

#define CRL(pin, cnfmode)  ((cnfmode) << (pin*4))
#define CRH(pin, cnfmode)  ((cnfmode) << ((pin-8)*4))

void hardware_init(void);
void i2c_setup(void);
void i2c1init(void);
void StartHSE(void);
void delay_ms(uint16_t ms);
void blink_pc13led(uint16_t freq);
void blink_pc14led(uint16_t freq);
void encoder_init(void);
char* my_itoa(int32_t num, char *str, uint8_t base);
int replace_char_at(char *str, size_t position, char character, uint8_t edtstrlen);
void pad_string_with_spaces(char *str, size_t current_len, size_t total_len);
void trim_and_clean_string(char *str, size_t max_len);
void safe_strncpy(char *dest, const char *src, size_t n);
void simple_srand(uint32_t seed);

void show_screensaver(void);

#endif // __COMMON_H__