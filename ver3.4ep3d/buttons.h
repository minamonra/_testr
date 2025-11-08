#ifndef __BUTTONS_H__
#define __BUTTONS_H__
#include "stm32f10x.h"


// Определения состояния кнопки
typedef enum { BTN_RELEASED = 0, BTN_PRESSED = 1 } button_state;

// Тип callback-функции для краткого и долгого нажатия
typedef void (*button_callback)(void);

// Структура для обработки кнопок
typedef struct {
  uint8_t stable_state;             // последнее стабильное состояние
  uint8_t last_state;               // предыдущее состояние
  uint8_t counter;                  // счётчик для дребезга
  uint16_t press_duration;          // длительность нажатия
  button_state state;               // текущее состояние кнопки
  uint8_t long_press_flag;          // флаг для long press
  button_callback on_press;         // callback для краткого нажатия
  button_callback on_long_press;    // callback для долгого нажатия
} button_debounce;

// Определения кнопок
#define NUM_BUTTONS 5
extern button_debounce buttons[NUM_BUTTONS];    // Используем extern для глобальной переменной

// Порог для дребезга и долгого нажатия
#define DEBOUNCE_THRESHOLD 5
#define LONG_PRESS_THRESHOLD 500    // Порог для long press (500 мс)

// Прототипы функций
void buttons_init(void);
void update_button_state(void);
void btn_press_handler1(void);         // Для BTN1
void btn_long_press_handler1(void);    // Для BTN1
void btn_press_handler2(void);         // Для BTN2
void btn_long_press_handler2(void);    // Для BTN2
void btn_press_handler3(void);         // Для BTN3
void btn_long_press_handler3(void);    // Для BTN3
void btn_press_handler4(void);         // Для BTN4
void btn_long_press_handler4(void);    // Для BTN4
void btn_press_handler5(void);         // Для ENC_BTN
void btn_long_press_handler5(void);    // Для ENC_BTN
uint8_t read_button(uint8_t button_id);

#endif // __BUTTONS_H__