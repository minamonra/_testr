#include "buttons.h"

// Определение глобальной переменной buttons
button_debounce buttons[NUM_BUTTONS];    // Убираем extern, так как это определение переменной

// Инициализация кнопок и их callback-функций
void buttons_init(void) {
  // Инициализация callback-функций для каждой кнопки
  buttons[0].on_press      = btn_press_handler2;         // BTN1 (поменяно с BTN2)
  buttons[0].on_long_press = btn_long_press_handler2;    // BTN1

  buttons[1].on_press      = btn_press_handler1;         // BTN2 (поменяно с BTN1)
  buttons[1].on_long_press = btn_long_press_handler1;    // BTN2

  buttons[2].on_press      = btn_press_handler3;         // BTN3
  buttons[2].on_long_press = btn_long_press_handler3;

  buttons[3].on_press      = btn_press_handler4;         // BTN4
  buttons[3].on_long_press = btn_long_press_handler4;

  buttons[4].on_press      = btn_press_handler5;         // ENC_BTN
  buttons[4].on_long_press = btn_long_press_handler5;
}

// Чтение состояния кнопки (для примера, если кнопка подключена к пину GPIO)
uint8_t read_button(uint8_t button_id) {
  if (button_id == 0) {
    return (GPIOA->IDR & GPIO_IDR_IDR8) ? 1 : 0;     // Проверка PA8
  } else if (button_id == 1) {
    return (GPIOB->IDR & GPIO_IDR_IDR14) ? 1 : 0;    // Проверка PB14
  } else if (button_id == 2) {
    return (GPIOA->IDR & GPIO_IDR_IDR11) ? 1 : 0;    // Проверка PA11
  } else if (button_id == 3) {
    return (GPIOA->IDR & GPIO_IDR_IDR12) ? 1 : 0;    // Проверка PA12
  } else if (button_id == 4) {
    return (GPIOB->IDR & GPIO_IDR_IDR1) ? 1 : 0;     // Проверка PB1 (Энкодер)
  }
  return 0;                                          // По умолчанию кнопки в "не нажатом" состоянии
}

// Обновление состояния кнопок
void update_button_state(void) {
  for (int i = 0; i < NUM_BUTTONS; i++) {
    uint8_t current_state = read_button(i);

    if (current_state == buttons[i].last_state) {
      if (buttons[i].counter < DEBOUNCE_THRESHOLD)
        buttons[i].counter++;
    } else {
      buttons[i].counter = 0;
    }

    buttons[i].last_state = current_state;

    if (buttons[i].counter >= DEBOUNCE_THRESHOLD) {
      if (current_state != buttons[i].stable_state) {
        buttons[i].stable_state = current_state;
        buttons[i].state        = (current_state == 0) ? BTN_PRESSED : BTN_RELEASED;

        // Вызов соответствующей callback функции при изменении состояния
        if (buttons[i].state == BTN_PRESSED) {
          // Если кнопка нажата, начинаем отсчёт времени
          buttons[i].press_duration = 0;
        } else if (buttons[i].state == BTN_RELEASED) {
          // Когда кнопка отпускается, проверяем длительность нажатия
          if (buttons[i].press_duration < LONG_PRESS_THRESHOLD) {
            // Короткое нажатие (если время нажатия меньше порога для долгого нажатия)
            if (buttons[i].on_press) {
              buttons[i].on_press();    // Вызов callback для краткого нажатия
            }
          } else {
            // Долгое нажатие
            if (buttons[i].on_long_press) {
              buttons[i].on_long_press();    // Вызов callback для долгого нажатия
            }
          }
          buttons[i].long_press_flag = 0;    // Сбрасываем флаг долгого нажатия
        }
      }
    }

    // Увеличиваем счётчик для долгого нажатия, если кнопка удерживается
    if (buttons[i].state == BTN_PRESSED) {
      buttons[i].press_duration++;
      if (buttons[i].press_duration >= LONG_PRESS_THRESHOLD && buttons[i].long_press_flag == 0) {
        buttons[i].long_press_flag = 1;
        if (buttons[i].on_long_press) {
          buttons[i].on_long_press();    // Вызов callback для долгого нажатия
        }
      }
    }
  }
}

// Eof button.c