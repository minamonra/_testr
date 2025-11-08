#include "common.h"
#include "stm32f10x.h"
#include <string.h>
#include "dispmt16s.h"

#define ENCBAAD

#ifdef ENCGOOD
#define ENCUPCNT 0x0D
#define ENCDNCNT 0x0E
#endif

#ifdef ENCBAAD
#define ENCUPCNT 0x0E
#define ENCDNCNT 0x0D
#endif


extern volatile uint32_t ttms;
volatile uint32_t ddms   = 0;
volatile uint32_t pc13ms = 0;
volatile uint32_t pc14ms = 0;
extern volatile uint32_t last_interrupt_time; // время последнего прерывания (в мс)
extern volatile uint8_t max_selected_index_enc ;  // теперь переменная, можно менять
extern volatile uint8_t selected_index_enc; // счётчик энкодера (8 бит, можно заменить на int32_t)

// Коллбек, вызывается при изменении значения
extern void (*encoder_callback)(uint8_t new_value, int8_t direction);
extern void update_button_state(void);


void iwdg_setup(void){
    uint32_t tmout = 16000000;
    /* Enable the peripheral clock RTC */
    /* (1) Enable the LSI (40kHz) */
    /* (2) Wait while it is not ready */
    RCC->CSR |= RCC_CSR_LSION; /* (1) */
    while((RCC->CSR & RCC_CSR_LSIRDY) != RCC_CSR_LSIRDY){if(--tmout == 0) break;} /* (2) */
    /* Configure IWDG */
    /* (1) Activate IWDG (not needed if done in option bytes) */
    /* (2) Enable write access to IWDG registers */
    /* (3) Set prescaler by 64 (1.6ms for each tick) */
    /* (4) Set reload value to have a rollover each 2s */
    /* (5) Check if flags are reset */
    /* (6) Refresh counter */
    IWDG->KR = IWDG_START; /* (1) */
    IWDG->KR = IWDG_WRITE_ACCESS; /* (2) */
    IWDG->PR = IWDG_PR_PR_1; /* (3) */
    IWDG->RLR = 1250; /* (4) */
    tmout = 16000000;
    while(IWDG->SR){if(--tmout == 0) break;} /* (5) */
    IWDG->KR = IWDG_REFRESH; /* (6) */
}


void hardware_init(void) {
  RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN | 
                  RCC_APB2ENR_IOPCEN | RCC_APB2ENR_AFIOEN;
  RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;    // Включаем тактирование GPIOB
  RCC->APB2ENR |= RCC_APB2ENR_USART1EN;  // Включаем тактирование USART1
  RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;    // Включаем тактирование I2C1

  // Отключить JTAG, оставить SWD
  AFIO->MAPR &= ~AFIO_MAPR_SWJ_CFG;
  AFIO->MAPR |= AFIO_MAPR_SWJ_CFG_JTAGDISABLE;

  if (SysTick_Config(72000)) { while(1); } // конфигурация SysTick = 1ms при тактовой 72MHz
  SysTick->LOAD |= SysTick_CTRL_ENABLE;    // включить SysTick

  // LED PC13
  GPIOC->CRH &= ~GPIO_CRH_CNF13;
  GPIOC->CRH |= CRH(13, CNF_PPOUTPUT | MODE_NORMAL);
  // LED PC14
  GPIOC->CRH &= ~GPIO_CRH_CNF14;
  GPIOC->CRH |= CRH(14, CNF_PPOUTPUT | MODE_NORMAL);
  // PC15-BUZZ
  GPIOC->CRH &= ~GPIO_CRH_CNF15;
  GPIOC->CRH |= CRH(15, CNF_PPOUTPUT | MODE_NORMAL);


  //Диспелей
  // PA15 (RS)
  GPIOA->CRH &= ~GPIO_CRH_CNF15;
  GPIOA->CRH |= CRH(15, CNF_PPOUTPUT | MODE_FAST);
  // PB3 (EN)
  GPIOB->CRL &= ~GPIO_CRL_CNF3;	  // Сбрасываем биты CNF
  GPIOB->CRL |= CRL(3, CNF_PPOUTPUT | MODE_FAST);
  // PB4 (D4)
  GPIOB->CRL &= ~GPIO_CRL_CNF4;
  GPIOB->CRL |= CRL(4, CNF_PPOUTPUT | MODE_FAST);
  // PB5 (D5)
  GPIOB->CRL &= ~GPIO_CRL_CNF5;
  GPIOB->CRL |= CRL(5, CNF_PPOUTPUT | MODE_FAST);
  // PB8 (D6)
  GPIOB->CRH &= ~GPIO_CRH_CNF8;
  GPIOB->CRH |= CRH(8, CNF_PPOUTPUT | MODE_FAST);
  // PB9 (D7)
  GPIOB->CRH &= ~GPIO_CRH_CNF9;
  GPIOB->CRH |= CRH(9, CNF_PPOUTPUT | MODE_FAST);

  // Настройка пинов PB6 (SCL) и PB7 (SDA)
  // PB6 (SCL) - AF Open-Drain, 50MHz
  GPIOB->CRL &= ~GPIO_CRL_CNF6;
  GPIOB->CRL |= CRL(6, CNF_AFOD | MODE_FAST);
  // PB7 (SDA) - AF Open-Drain, 50MHz
  GPIOB->CRL &= ~GPIO_CRL_CNF7;
  GPIOB->CRL |= CRL(7, CNF_AFOD | MODE_FAST);

  // Настроим кнопки PB14, PA8, PA11, PA12, PB1
  // PB14 — кнопка 1
  GPIOB->CRH &= ~(GPIO_CRH_MODE14 | GPIO_CRH_CNF14);
  GPIOB->CRH |= CRH(14, CNF_PUDINPUT | MODE_INPUT);
  GPIOB->ODR |= GPIO_ODR_ODR14; // подтяжка вверх

  // PA8 — кнопка 2
  GPIOA->CRH &= ~(GPIO_CRH_MODE8 | GPIO_CRH_CNF8);
  GPIOA->CRH |= CRH(8, CNF_PUDINPUT | MODE_INPUT);
  GPIOA->ODR |= GPIO_ODR_ODR8; // подтяжка вверх

  // PA11 — кнопка 3
  GPIOA->CRH &= ~(GPIO_CRH_MODE11 | GPIO_CRH_CNF11);
  GPIOA->CRH |= CRH(11, CNF_PUDINPUT | MODE_INPUT);
  GPIOA->ODR |= GPIO_ODR_ODR11; // подтяжка вверх

  // PA12 — кнопка 4
  GPIOA->CRH &= ~(GPIO_CRH_MODE12 | GPIO_CRH_CNF12);
  GPIOA->CRH |= CRH(12, CNF_PUDINPUT | MODE_INPUT);
  GPIOA->ODR |= GPIO_ODR_ODR12; // подтяжка вверх

  // PB1 — кнопка энкодера
  GPIOB->CRL &= ~(GPIO_CRL_MODE1 | GPIO_CRL_CNF1);
  GPIOB->CRL |= CRL(1, CNF_PUDINPUT | MODE_INPUT);
  GPIOB->ODR |= GPIO_ODR_ODR1; // подтяжка вверх

  // PB15 DE: General purpose output push-pull 50MHz
  GPIOB->CRH &= ~GPIO_CRH_CNF15;
  GPIOB->CRH |= CRH(15, CNF_PPOUTPUT | MODE_NORMAL);

}

void i2c1init(void) {
  // Сброс I2C1
  I2C1->CR1 = I2C_CR1_SWRST;
  I2C1->CR1 = 0;
  // Устанавливаем частоту APB1 в MHz
  I2C1->CR2 = 36;
  // CCR для 100kHz: 36 MHz / (2*100kHz) = 180
  I2C1->CCR = 180;
  // TRISE = 36 + 1 = 37
  I2C1->TRISE = 37;
  // Включаем I2C и ACK
  I2C1->CR1 = I2C_CR1_PE | I2C_CR1_ACK;
}

void encoder_init(void) {
  // Тактирование GPIOB
  RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;
  // Настройка PB12 и PB13 как входов с подтяжкой вверх (CNF=10, MODE=00 → 0x8)
  GPIOB->CRH &= ~((0xF << ((12 - 8) * 4)) | (0xF << ((13 - 8) * 4)));
  GPIOB->CRH |=  ((0x8 << ((12 - 8) * 4)) | (0x8 << ((13 - 8) * 4)));
  // Включаем подтяжку вверх
  GPIOB->ODR |= (1 << 12) | (1 << 13);

}

void StartHSE(void) {
  __IO uint32_t StartUpCounter = 0;
  // SYSCLK, HCLK, PCLK2 and PCLK1 configuration
  RCC->CR |= ((uint32_t)RCC_CR_HSEON); // Enable HSE
  // Wait till HSE is ready and if Time out is reached exit
  do {
    ++StartUpCounter;
  } while(!(RCC->CR & RCC_CR_HSERDY) && (StartUpCounter < 10000));
  if (RCC->CR & RCC_CR_HSERDY) // HSE started
  {
    FLASH->ACR |= FLASH_ACR_PRFTBE; // Enable Prefetch Buffer
    FLASH->ACR &= (uint32_t)((uint32_t)~FLASH_ACR_LATENCY);
    FLASH->ACR |= (uint32_t)FLASH_ACR_LATENCY_2;    // Flash 2 wait state
    RCC->CFGR |= (uint32_t)RCC_CFGR_HPRE_DIV1;// HCLK = SYSCLK
    RCC->CFGR |= (uint32_t)RCC_CFGR_PPRE2_DIV1;// PCLK2 = HCLK
    RCC->CFGR |= (uint32_t)RCC_CFGR_PPRE1_DIV2;// PCLK1 = HCLK
    //  PLL configuration: PLLCLK = HSE * 9 = 72 MHz
    RCC->CFGR &= (uint32_t)((uint32_t)~(RCC_CFGR_PLLSRC | RCC_CFGR_PLLXTPRE | RCC_CFGR_PLLMULL));
    RCC->CFGR |= (uint32_t)(RCC_CFGR_PLLSRC_HSE | RCC_CFGR_PLLMULL9);   //
    RCC->CR |= RCC_CR_PLLON;// Enable PLL
    // Wait till PLL is ready
    StartUpCounter = 0;
    while((RCC->CR & RCC_CR_PLLRDY) == 0 && ++StartUpCounter < 1000){}
    // Select PLL as system clock source
    RCC->CFGR &= (uint32_t)((uint32_t)~(RCC_CFGR_SW));
    RCC->CFGR |= (uint32_t)RCC_CFGR_SW_PLL;    
    // Wait till PLL is used as system clock source
    StartUpCounter = 0;
    while(((RCC->CFGR & (uint32_t)RCC_CFGR_SWS) != (uint32_t)0x08) && ++StartUpCounter < 1000){}
  }
  else // HSE fails to start-up
  { 
        ; // add some code here (use HSI)
  }  
}

//// Логика чтения энкодера, вызывается из SysTick (каждые ~2 мс)
void encoder_poll(void) {
    static uint8_t ABs = 0;
    static uint8_t last_ABs = 0;

    ABs = (ABs << 2) & 0x0F;

    uint8_t A = (GPIOB->IDR & (1 << 12)) ? 1 : 0;
    uint8_t B = (GPIOB->IDR & (1 << 13)) ? 1 : 0;

    ABs |= (B << 1) | A;

    int8_t direction = 0;

    switch (ABs) {
    case ENCUPCNT:  // +1
        if (selected_index_enc >= max_selected_index_enc )
            selected_index_enc = 0;
        else
            selected_index_enc++;
        direction = +1;
        break;
    case ENCDNCNT:  // -1
        if (selected_index_enc == 0)
            selected_index_enc = max_selected_index_enc ;
        else
            selected_index_enc--;
        direction = -1;
        break;
    default:
        direction = 0;
        break;
    }

    // Защита от выхода за границы
    if (selected_index_enc > max_selected_index_enc )
        selected_index_enc = max_selected_index_enc ;

    if (direction != 0 && encoder_callback != 0) {
        encoder_callback(selected_index_enc, direction);
    }
} // void encoder_poll(void)

// прерывание системного тикера
void SysTick_Handler(void) 
{
  ++ttms;
  if (ddms) ddms--;

   static uint8_t encoder_timer = 0;
    encoder_timer++;
    if (encoder_timer >= 2) {  // каждые ~2 мс
        encoder_timer = 0;
        encoder_poll();
    }
  update_button_state();// Опрос кнопок каждый 1 мс

}

// Мигалка PC13
void blink_pc13led(uint16_t freq)
{
  if (pc13ms > ttms || ttms - pc13ms > freq)
  {
    LED2TOGGLE;
    pc13ms = ttms;
  }
}

// Мигалка PC14
void blink_pc14led(uint16_t freq)
{
  if (pc14ms > ttms || ttms - pc14ms > freq)
  {
    LED1TOGGLE;
    pc14ms = ttms;
    
  }
}

// Задержка через SysTick
void delay_ms(uint16_t ms)
{
  ddms = ms;
  do {} while (ddms);
}

// Заменяет символ в строке по позиции
int replace_char_at(char *str, size_t position, char character, uint8_t edtstrlen) {
  if (!str) return -1;
  if (position >= edtstrlen) return -1;
  str[position] = character;
  return 0;
}


// Выравнивает строку пробелами до total_len (если нужно)
void pad_string_with_spaces(char *str, size_t current_len, size_t total_len) {
    if (!str) return;  // добавить проверку на NULL
    if (current_len >= total_len) return;
    // Предполагаем, что буфер достаточно большой (total_len + 1)
    for (size_t i = current_len; i < total_len; i++) {
        str[i] = 0x20;
    }
    str[total_len] = '\0';
}

// Удаляет пробелы с конца строки и обрезает по max_len
void trim_and_clean_string(char *str, size_t max_len) {
  if (!str) return;
  size_t len = strlen(str);
  if (len > max_len) {
    str[max_len] = '\0';
    len = max_len;
  }
  while (len > 0 && str[len - 1] == 0x20) {
    str[len - 1] = '\0';
    len--;
  }
}

// Простой генератор случайных чисел
static uint32_t random_seed = 0;
void simple_srand(uint32_t seed) { random_seed = seed; }
uint16_t simple_rand(void) {
    random_seed = (random_seed * 1103515245 + 12345) & 0x7FFFFFFF;
    return (uint16_t)random_seed;
}

// Безопасная функция копирования строки
void safe_strncpy(char *dest, const char *src, size_t n) {
  if (dest == NULL || src == NULL || n == 0) return;
  char *d = dest;
  const char *s = src;
  // Копируем символы пока есть место и не конец строки
  while (n > 1 && *s != '\0') {
    *d++ = *s++;
    n--;
  }
  *d = '\0'; // Всегда завершаем строку нулём
}

// Хранитель экрана by ChatGPT и моими изменениями :)
void show_screensaver(void) {
  static uint8_t animation_frame      = 0;
  static uint8_t positions[2][16]     = {0};
  static uint8_t directions[2][16]    = {0};
  static uint8_t new_positions[2][16] = {0};    // Буфер для новых позиций

  // Символы для скринсейвера
  const uint8_t symbols[] = {'*', '.', '+', 'o', 'x', ':', '-', '=', '#', '@', 'O', 'X', 'H', '[', ']', '_'};

  // Инициализируем буфер новых позиций
  for (uint8_t line = 0; line < 2; line++) {
    for (uint8_t pos = 0; pos < 16; pos++) {
      new_positions[line][pos] = 0;
    }
  }

  // Каждый 3-й кадр обновляем анимацию
  if (animation_frame % 4 == 0) {
    // Сначала обновляем все позиции в буфере
    for (uint8_t line = 0; line < 2; line++) {
      for (uint8_t pos = 0; pos < 16; pos++) {
        if (positions[line][pos] != 0) {
          // Определяем новую позицию
          uint8_t new_pos;
          if (directions[line][pos] == 0) {
            // Движение вправо
            new_pos = (pos < 15) ? pos + 1 : pos;
            if (new_pos == pos && simple_rand() % 2 == 0) {
              // Достигли края - меняем направление
              directions[line][pos] = 1;
            }
          } else {
            // Движение влево
            new_pos = (pos > 0) ? pos - 1 : pos;
            if (new_pos == pos && simple_rand() % 2 == 0) {
              // Достигли края - меняем направление
              directions[line][pos] = 0;
            }
          }

          // Если новая позиция свободна, перемещаем символ
          if (new_positions[line][new_pos] == 0) {
            new_positions[line][new_pos] = positions[line][pos];
          }
        }
      }
    }

    // Копируем буфер обратно в positions
    for (uint8_t line = 0; line < 2; line++) {
      for (uint8_t pos = 0; pos < 16; pos++) {
        positions[line][pos] = new_positions[line][pos];
      }
    }

    // Добавляем новые символы
    if (simple_rand() % 5 == 0) {
      uint8_t line = simple_rand() % 2;
      uint8_t pos  = simple_rand() % 16;

      if (positions[line][pos] == 0) {
        positions[line][pos]  = symbols[simple_rand() % (sizeof(symbols))];
        directions[line][pos] = simple_rand() % 2;
      }
    }

    // Очищаем экран
    lcdString16("", 0);
    lcdString16("", 1);

    // Отображаем все символы
    for (uint8_t line = 0; line < 2; line++) {
      for (uint8_t pos = 0; pos < 16; pos++) {
        if (positions[line][pos] != 0) {
          lcdSetCursor(pos, line);
          lcdChar(positions[line][pos]);
        }
      }
    }

    // Иногда добавляем "вспышку" - несколько символов сразу
    if (simple_rand() % 20 == 0) {
      uint8_t line      = simple_rand() % 2;
      uint8_t start_pos = simple_rand() % 8;

      for (uint8_t i = 0; i < 4; i++) {
        uint8_t pos           = (start_pos + i) % 16;
        positions[line][pos]  = symbols[simple_rand() % (sizeof(symbols))];
        directions[line][pos] = simple_rand() % 2;
        lcdSetCursor(pos, line);
        lcdChar(positions[line][pos]);
      }
    }
  }

  animation_frame++;

  // Периодически сбрасываем анимацию
  if (animation_frame > 250) {
    animation_frame = 0;
    // Очищаем все позиции
    for (uint8_t line = 0; line < 2; line++) {
      for (uint8_t pos = 0; pos < 16; pos++) {
        positions[line][pos] = 0;
      }
    }
    lcdString16("", 0); // затираем прошлое
    lcdString16("", 1); // вместо долгого lcdClean
  }
} // void show_screensaver(void)

// Eof common.c
