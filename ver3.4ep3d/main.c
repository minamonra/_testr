#include <string.h>
#include "cp1251_chars.h"
#include "common.h"
#include "dispmt16s.h"
#include "buttons.h"
#include "eeprom.h"
#include "rs485.h"
#define PULTVERSION "ver.3.4.ep3d"

// Режимы работы дисплея
typedef enum {
    DNORMMODE    = 0, // Нормальный режим
    DEDITMODE    = 1, // Режим редактирования
    SHOWMSG      = 2, // Режим показа сообщений
    DBRIGHTMODE  = 3, // Режим настройки яркости
    DSETUPMODE   = 4, // Режим настройки
    DSCREENSAVER = 5  // Режим скринсейвера
} display_mode_t;

// Коды ошибок
typedef enum {
    ERR_OK = 0,
    ERR_READ = -1,
    ERR_OUT = -2
} error_code_t;

#define MSGSHOWTNS            600 // Время показа сообщения в миллисекундах
#define EDIT_TIMEOUT_MS     50000 // Таймаут редактирования
#define NORMMODE_TIMEOUT_MS 50000 // Таймаут бездействия в нормальном режиме

// Адреса в EEPROM
#define EEBRIGHTNESS   1   // Ячейка для хранения яркости
#define EELASTUSEDCELL 2   // Ячейка с последним выбранным индексом
#define EEFIRSTTIMERUN 3   // Ячейка инициализации EEPROM, первый запуск там 0xFF, после = 0x00
#define EENEED2SAVEON  4   // Задумана как нужность сохранения в памяти панели последнего посланного текста

// Параметры редактирования
#define EDTSTRLEN        15  // Длина редактируемой строки для отображения (16 символов + терминатор)
#define EDTMODMAXCNT     55  // Максимальное значение для режима редактирования
#define MAXCELLEEPROMUSE 50  // Максимальное количество ячеек EEPROM
#define MAX_BRIGHTNESS   9   // Максимальная яркость

// Глобальные переменные состояния
volatile uint8_t selected_index_enc            = 0;
volatile uint8_t max_selected_index_enc        = MAXCELLEEPROMUSE;
volatile uint32_t ttms                         = 0;
volatile uint32_t lcdms                        = 0;
volatile display_mode_t display_mode           = DNORMMODE;
volatile uint8_t symbol_position               = 0;
volatile uint8_t current_sell_ed               = 0;
volatile uint8_t brightness                    = 5;
volatile uint8_t last_show_editing_cell        = 0;
volatile uint32_t last_edit_activity_time      = 0;
volatile uint32_t last_norm_mode_activity_time = 0;
volatile uint32_t last_bright_activity_time    = 0;
volatile uint8_t display_part                  = 0; // 0 - первая часть строки, 1 - вторая часть
volatile uint8_t editing_part                  = 0; // 0 - редактируем первую часть, 1 - редактируем вторую часть

// Буферы для строк - увеличены для работы с 32-символьными строками из EEPROM
char editing_string[33] = {0};  // 32 символа + терминатор
char display_string[33] = {0};  // 32 символа + терминатор
char new_char           = 0x20;
char msg_text[32]       = {0};

// Переменные для управления сообщениями
volatile uint32_t msg_start_time = 0;
volatile uint8_t msg_active      = 0;

// Коллбек функция для обработки вращения энкодера
void (*encoder_callback)(uint8_t new_value, int8_t direction) = 0;

// Структура для кэширования данных EEPROM
typedef struct {
    uint8_t index;
    char data[33];  // Увеличено до 32 символов + терминатор
    uint8_t valid;
} eeprom_cache_t;

static eeprom_cache_t eeprom_cache = {0xFF, {0}, 0};

// Индексы системных сообщений
typedef enum {
    MSG_FUCK_OFF = 0,
    MSG_WOW_NICE,
    MSG_AMAZING_BRIGHT,
    MSG_VERSION,
    MSG_EDIT_CELL,
    MSG_CELL,
    MSG_BRIGHTNESS_SHORT,
    MSG_BRIGHTNESS_FULL,
    MSG_SETUP,
    MSG_ERR_READ,
    MSG_ERR_OUT,
    MSG_COUNT
} message_id_t;

// Тексты системных сообщений
static const char messages[MSG_COUNT][32] = {
    [MSG_FUCK_OFF]         = "Отмена!",
    [MSG_WOW_NICE]         = "Сделал!",
    [MSG_AMAZING_BRIGHT]   = "Изменил!",
    [MSG_VERSION]          = PULTVERSION,
    [MSG_EDIT_CELL]        = "Ред яч:",
    [MSG_CELL]             = "Яч:",
    [MSG_BRIGHTNESS_SHORT] = " Ярк:",
    [MSG_BRIGHTNESS_FULL]  = "Яркость: ",
    [MSG_SETUP]            = "Уст:",
    [MSG_ERR_READ]         = "Err read",
    [MSG_ERR_OUT]          = "Err out"
};

// Поиск индекса символа в таблице CP1251
int find_cp1251_index(char c) {
    for (int i = 0; cp1251_chars[i] != 0; i++) {
        if (cp1251_chars[i] == (unsigned char)c) return i;
    }
    return 0; // если не найден — возвращаем пробел
}

// Кэшированное чтение EEPROM
error_code_t cached_eeprom_read_string(uint8_t index, char *buf) {
    if (!buf) return ERR_READ;
    
    // Всегда очищаем буфер перед заполнением
    memset(buf, ' ', 32);  // Увеличено до 32
    buf[32] = '\0';        // Увеличено до 32
    
    // Проверяем кэш
    if (eeprom_cache.valid && eeprom_cache.index == index) {
        safe_strncpy(buf, eeprom_cache.data, 32);  // Увеличено до 32
        int len = strlen(buf);
        for (int i = len; i < 32; i++) {  // Увеличено до 32
            buf[i] = ' ';
        }
        buf[32] = '\0';  // Увеличено до 32
        return ERR_OK;
    }
    
    // Читаем из EEPROM
int err = eeprom_read_string_by_num(index, buf);
if (err == 0) {
    // Гарантируем, что буфер полностью заполнен и не обрезан по '\0'
    for (int i = 0; i < 32; i++) {
        if (buf[i] == '\0' || buf[i] == 0x00 || buf[i] == 0xFF) buf[i] = ' ';
    }
    buf[32] = '\0';
    memcpy(eeprom_cache.data, buf, 33);
    eeprom_cache.index = index;
    eeprom_cache.valid = 1;
}
    return (error_code_t)err;
}

// Сброс кэша EEPROM для указанного индекса
void invalidate_eeprom_cache(uint8_t index) {
    if (eeprom_cache.valid && eeprom_cache.index == index) {
        eeprom_cache.valid = 0;
    }
}

// Функции обновления времени последней активности
static inline void update_edit_activity_time(void) {
    last_edit_activity_time = ttms;
}

static inline void update_norm_mode_activity_time(void) {
    last_norm_mode_activity_time = ttms;
}

static inline void update_bright_activity_time(void) {
    last_bright_activity_time = ttms;
}

// Общая функция показа сообщения об отмене
static void show_cancel_message(void) {
    display_mode = SHOWMSG;
    max_selected_index_enc = MAXCELLEEPROMUSE;
    lcdClearViaChars();
    safe_strncpy(msg_text, messages[MSG_FUCK_OFF], sizeof(msg_text));
    msg_start_time = ttms;
    msg_active = 1;
}

// Проверка таймаутов - объединенная функция
void check_timeouts(void) {
    switch (display_mode) {
        case DEDITMODE:
            if (ttms - last_edit_activity_time > EDIT_TIMEOUT_MS) {
                selected_index_enc = current_sell_ed;
                show_cancel_message();
            }
            break;
            
        case DNORMMODE:
            if (ttms - last_norm_mode_activity_time > NORMMODE_TIMEOUT_MS) {
                display_mode = DSCREENSAVER;
                lcdClearViaChars();
            }
            break;
            
        case DBRIGHTMODE:
            if (ttms - last_bright_activity_time > EDIT_TIMEOUT_MS) {
                selected_index_enc = last_show_editing_cell;
                show_cancel_message();
            }
            break;
            
        default:
            break;
    }
}

// Общая функция выхода из скринсейвера
static inline void exit_screensaver(void) {
    display_mode = DNORMMODE;
    update_norm_mode_activity_time();
    lcdClearViaChars();
}

// Общая функция чтения и отображения ячейки EEPROM
static void read_and_display_cell(uint8_t cell_index) {
    char buf[33] = {0};  // Увеличено до 32 + терминатор
    error_code_t err = cached_eeprom_read_string(cell_index, buf);
    
    switch (err) {
        case ERR_READ:
            safe_strncpy(display_string, messages[MSG_ERR_READ], sizeof(display_string));
            break;
        case ERR_OUT:
            safe_strncpy(display_string, messages[MSG_ERR_OUT], sizeof(display_string));
            break;
        case ERR_OK:
        default:
            safe_strncpy(display_string, buf, sizeof(display_string));
            // Не обрезаем строку, так как работаем с полными 32 символами
            break;
    }
}

// Функция для получения отображаемой части строки
static void get_display_part(char *output) {
    if (display_part == 0) {
        // Первая часть строки (символы 0-15)
        safe_strncpy(output, display_string, 16);
    } else {
        // Вторая часть строки (символы 16-31)
        if (strlen(display_string) > 16) {
            safe_strncpy(output, display_string + 16, 16);
        } else {
            // Если строка короче 16 символов, показываем пустую вторую часть
            memset(output, ' ', 16);
            output[16] = '\0';
        }
    }
}

// Обработчики кнопок - оптимизированные версии

// Короткое нажатие кнопки 1
void btn_press_handler1(void) {
    if (display_mode == DSCREENSAVER) {
        exit_screensaver();
        return;
    }
    
    switch (display_mode) {
        case DEDITMODE:
            update_edit_activity_time();
            // Перемещаем позицию курсора влево с зацикливанием
            symbol_position = (symbol_position == 0) ? EDTSTRLEN - 1 : symbol_position - 1;
            new_char = editing_string[symbol_position];
            selected_index_enc = find_cp1251_index(new_char);
            break;
            
        case DNORMMODE:
            update_norm_mode_activity_time();
            // Перемещаемся к предыдущей ячейке EEPROM
            selected_index_enc = (selected_index_enc == 0) ? max_selected_index_enc - 1 : selected_index_enc - 1;
            read_and_display_cell(selected_index_enc);
            display_part = 0; // Сбрасываем к первой части при смене ячейки
            break;
            
        default:
            break;
    }
}

// Длинное нажатие кнопки 1
void btn_long_press_handler1(void) {
    if (display_mode == DSCREENSAVER) {
        exit_screensaver();
        return;
    }
    
    if (display_mode == DNORMMODE) {
        update_norm_mode_activity_time();
        
        // Вход в режим редактирования текущей отображаемой части
        current_sell_ed = selected_index_enc;
        display_mode = DEDITMODE;
        max_selected_index_enc = EDTMODMAXCNT;
        
        // Запоминаем, какую часть редактируем
        editing_part = display_part;
        
        // Подготовка строки для редактирования - берем соответствующую часть
        if (editing_part == 0) {
            // Редактируем первую часть (символы 0-15)
            safe_strncpy(editing_string, display_string, 16);
        } else {
            // Редактируем вторую часть (символы 16-31)
            if (strlen(display_string) > 16) {
                safe_strncpy(editing_string, display_string + 16, 16);
            } else {
                // Если строка короче 16 символов, начинаем с пустой второй части
                memset(editing_string, ' ', 16);
                editing_string[16] = '\0';
            }
        }
        
        trim_and_clean_string(editing_string, EDTSTRLEN);
        pad_string_with_spaces(editing_string, strlen(editing_string), EDTSTRLEN);
        
        // Установка начальной позиции редактирования
        symbol_position = 0;
        new_char = editing_string[symbol_position];
        selected_index_enc = find_cp1251_index(new_char);
        
        update_edit_activity_time();
        lcdClearViaChars();
    }
}

// Короткое нажатие кнопки 2
void btn_press_handler2(void) {
    if (display_mode == DSCREENSAVER) {
        exit_screensaver();
        return;
    }
    
    switch (display_mode) {
        case DEDITMODE:
            update_edit_activity_time();
            // Перемещаем позицию курсора вправо с зацикливанием
            symbol_position = (symbol_position >= EDTSTRLEN - 1) ? 0 : symbol_position + 1;
            new_char = editing_string[symbol_position];
            selected_index_enc = find_cp1251_index(new_char);
            break;
            
        case DNORMMODE:
            update_norm_mode_activity_time();
            // Перемещаемся к следующей ячейке EEPROM
            selected_index_enc = (selected_index_enc >= max_selected_index_enc - 1) ? 0 : selected_index_enc + 1;
            read_and_display_cell(selected_index_enc);
            display_part = 0; // Сбрасываем к первой части при смене ячейки
            break;
            
        default:
            break;
    }
}

// Длинное нажатие кнопки 2
void btn_long_press_handler2(void) {
    if (display_mode == DSCREENSAVER) {
        exit_screensaver();
        return;
    }
    
    if (display_mode == DNORMMODE) {
        update_norm_mode_activity_time();
        
        // Вход в режим настройки яркости
        display_mode = DBRIGHTMODE;
        max_selected_index_enc = MAX_BRIGHTNESS;
        last_show_editing_cell = selected_index_enc;
        selected_index_enc = brightness;
        
        update_bright_activity_time();
        lcdClearViaChars();
    }
}

// Короткое нажатие кнопки 3 - переключение между частями строки
void btn_press_handler3(void) {
    if (display_mode == DSCREENSAVER) {
        exit_screensaver();
        return;
    }
    
    if (display_mode == DNORMMODE) {
        update_norm_mode_activity_time();
        // Переключаем между первой и второй частью строки
        display_part = !display_part;
    }
}

// Длинное нажатие кнопки 3 не используется
void btn_long_press_handler3(void) {
    // Длинное нажатие кнопки 3 не используется
}

// Короткое нажатие кнопки 4 (Send, Ok)
void btn_press_handler4(void) {
    if (display_mode == DSCREENSAVER) {
        exit_screensaver();
        return;
    }
    
    switch (display_mode) {
        case DEDITMODE:
            update_edit_activity_time();
            // Сохранение отредактированной строки
            display_mode = SHOWMSG;
            max_selected_index_enc = MAXCELLEEPROMUSE;
            selected_index_enc = current_sell_ed;
            trim_and_clean_string(editing_string, EDTSTRLEN);
            
            // Сохраняем в соответствующую часть строки
            char temp_full_string[33] = {0};
            
            // Сначала читаем текущую полную строку из EEPROM
            if (cached_eeprom_read_string(selected_index_enc, temp_full_string) != ERR_OK) {
                // Если не удалось прочитать, используем текущий display_string
                safe_strncpy(temp_full_string, display_string, 32);
            } else {
    // защита от преждевременного '\0' внутри первой половиныl
    for (int i = 0; i < 32; i++) {
        if (temp_full_string[i] == '\0' || temp_full_string[i] == 0x00) {
            temp_full_string[i] = ' ';
        }
    }
    temp_full_string[32] = '\0';
}
            
            if (editing_part == 0) {
                // Сохраняем в первую часть (символы 0-15)
                // Копируем только 16 символов, не трогая остальные
                for (int i = 0; i < 16 && i < EDTSTRLEN; i++) {
                    temp_full_string[i] = editing_string[i];
                }
            } else {
                // Сохраняем во вторую часть (символы 16-31)
                for (int i = 0; i < 16 && i < EDTSTRLEN; i++) {
                    temp_full_string[16 + i] = editing_string[i];
                }
            }
            
            // Гарантируем, что строка имеет правильную длину
            temp_full_string[32] = '\0';
            
            eeprom_write_string_by_num(selected_index_enc, temp_full_string);
            invalidate_eeprom_cache(selected_index_enc);
            
            // Обновляем display_string для отображения
            safe_strncpy(display_string, temp_full_string, 32);
            display_string[32] = '\0';
            
            lcdClearViaChars();
            safe_strncpy(msg_text, messages[MSG_WOW_NICE], sizeof(msg_text));
            msg_start_time = ttms;
            msg_active = 1;
            break;
            
        case DNORMMODE:
            update_norm_mode_activity_time();
            // Отправка данных через RS485 - отправляем полную строку
            rs485_send_string_with_params(brightness, '#', '#', cp1251_to_utf8_alloc(display_string));
            eeprom_write_uint16_by_num(EELASTUSEDCELL, selected_index_enc);
            break;
            
        case DBRIGHTMODE:
            update_bright_activity_time();
            // Сохранение новой настройки яркости
            brightness = selected_index_enc;
            display_mode = SHOWMSG;
            max_selected_index_enc = MAXCELLEEPROMUSE;
            selected_index_enc = (selected_index_enc > max_selected_index_enc) ? 0 : last_show_editing_cell;
            eeprom_write_uint16_by_num(EEBRIGHTNESS, brightness);
            lcdClearViaChars();
            safe_strncpy(msg_text, messages[MSG_AMAZING_BRIGHT], sizeof(msg_text));
            msg_start_time = ttms;
            msg_active = 1;
            break;
            
        default:
            break;
    }
}

// Длинное нажатие кнопки 4 не используется 
void btn_long_press_handler4(void) {}

// Короткое нажатие кнопки 5 (Send)
void btn_press_handler5(void) {
    if (display_mode == DSCREENSAVER) {
        exit_screensaver();
        return;
    }
    
    if (display_mode == DNORMMODE) {
        update_norm_mode_activity_time();
    }
    eeprom_clear_all_strings();
    // Отправка данных через RS485
    rs485_send_string_with_params(brightness, '#', '#', cp1251_to_utf8_alloc(display_string));
    eeprom_write_uint16_by_num(EELASTUSEDCELL, selected_index_enc);
}

// Длинное нажатие кнопки 5 не используется
void btn_long_press_handler5(void) {}

// Переменные для управления обновлением дисплея
static display_mode_t last_display_mode = (display_mode_t)0xFF;
volatile uint8_t force_display_update = 0;

// Основная функция отрисовки интерфейса
void display_process(void) {
    if (lcdms > ttms || ttms - lcdms > 20) {
        // Проверяем изменение режима для полной перерисовки
        if (display_mode != last_display_mode) {
            lcdClear();
            last_display_mode = display_mode;
            force_display_update = 0;
        }
        
        // Проверяем таймауты
        check_timeouts();
        
        switch (display_mode) {
            case SHOWMSG:
                // Обработка режима показа сообщений
                if (msg_active == 0) {
                    msg_start_time = ttms;
                    msg_active = 1;
                }
                if (ttms - msg_start_time > MSGSHOWTNS) {
                    // Возврат в нормальный режим после показа сообщения
                    msg_active = 0;
                    display_mode = DNORMMODE;
                    update_norm_mode_activity_time();
                    lcdClear();
                    cached_eeprom_read_string(selected_index_enc, display_string);
                    lcdString16(display_string, 1);
                    lcdms = ttms - 21; // Принудительное обновление на следующем цикле
                } else {
                    // Показ сообщения
                    lcdSetCursorB(0, 0, 0);
                    lcdPrintUtf8(msg_text, 0);
                }
                break;
                
            case DEDITMODE:
                // Режим редактирования - отображаем редактируемую часть
                lcdSetCursor(0, 0);
                lcdPrintUtf8(messages[MSG_EDIT_CELL], 0);
                lcdPrintTwoDigitNumber(current_sell_ed);
                
                // Показываем какую часть редактируем
                if (editing_part == 0) {
                    lcdPrintUtf8("(1)", 0);
                } else {
                    lcdPrintUtf8("(2)", 0);
                }
                
                lcdString16(editing_string, 1);
                lcdSetCursorN(symbol_position, 1, 2);
                break;
                
            case DNORMMODE:
                // Нормальный режим - отображаем текущую часть строки
                lcdSetCursor(0, 0);
                lcdPrintUtf8(messages[MSG_CELL], 0);
                lcdPrintTwoDigitNumber(selected_index_enc);
                lcdPrintUtf8(messages[MSG_BRIGHTNESS_SHORT], 0);
                lcdPrintTwoDigitNumber(brightness);
                
                // Отображаем текущую часть строки
                char current_part[17] = {0};
                get_display_part(current_part);
                lcdString16(current_part, 1);
                
                // Показываем индикатор части строки
                lcdSetCursor(15, 0);
                if (display_part == 0) {
                    lcdData('.');
                } else {
                    lcdData(':');
                }
                break;
                
            case DBRIGHTMODE:
                // Режим настройки яркости
                lcdSetCursor(0, 0);
                lcdPrintUtf8(messages[MSG_BRIGHTNESS_FULL], 0);
                lcdPrintTwoDigitNumber(selected_index_enc);
                break;
                
            case DSETUPMODE:
                // Режим настройки (заглушка)
                lcdSetCursor(0, 0);
                lcdPrintUtf8(messages[MSG_SETUP], 0);
                break;
                
            case DSCREENSAVER:
                // Режим скринсейвера - показ случайных символов
                show_screensaver();
                break;
        }
        lcdms = ttms;
    }
}

// Обработчик вращения энкодера
void my_encoder_handler(uint8_t new_value, int8_t direction) {
    switch (display_mode) {
        case DEDITMODE:
            update_edit_activity_time();
            // Изменение символа в режиме редактирования
            if (selected_index_enc >= sizeof(cp1251_chars) - 1) {
                selected_index_enc = 0;
            }
            new_char = cp1251_chars[selected_index_enc];
            replace_char_at(editing_string, symbol_position, new_char, EDTSTRLEN);
            break;
            
        case DNORMMODE:
            update_norm_mode_activity_time();
            read_and_display_cell(selected_index_enc);
            break;
            
        case DBRIGHTMODE:
            update_bright_activity_time();
            // Обработка настройки яркости (энкодер изменяет selected_index_enc)
            break;
            
        case DSETUPMODE:
        default:
            // Обработка других режимов
            break;
    }
}

// Главная функция
int main(void) {
    // Инициализация системы
    StartHSE();
    hardware_init();
    i2c1init();
    
    delay_ms(20);
    lcd_init();
    delay_ms(20);
    lcd_init();
    delay_ms(20);
    lcd_init();

    // Инициализация периферии
    encoder_init();
    encoder_callback = my_encoder_handler;
    buttons_init();
    
    // Инициализация генератора случайных чисел
    simple_srand(ttms);
    
    // Загрузка сохраненных настроек
    uint16_t tmp;
    if (eeprom_read_uint16_by_num(EEBRIGHTNESS, &tmp) == 0 && tmp <= MAX_BRIGHTNESS) {
        brightness = (uint8_t)tmp;
    }
    if (eeprom_read_uint16_by_num(EELASTUSEDCELL, &tmp) == 0 && tmp < MAXCELLEEPROMUSE) {
        selected_index_enc = (uint8_t)tmp;
    }
    
    // Загрузка и подготовка строки для отображения
    cached_eeprom_read_string(selected_index_enc, display_string);
    
    // Инициализация времени активности
    update_norm_mode_activity_time();
    update_bright_activity_time();
    delay_ms(10);
    // Инициализация RS485
    rs485_init(4800);
    delay_ms(15);
    
    // Показ версии и начальная инициализация
    lcdPrintUtf8(messages[MSG_VERSION], 0);
    delay_ms(1500);
    lcdClearViaChars();
    
    // Проверка кнопки для входа в режим настройки
    if (!(GPIOB->IDR & GPIO_IDR_IDR14)) {
        display_mode = DSETUPMODE;
    }
    
    // Отправка начальных данных и сохранение состояния
    rs485_send_string_with_params(brightness, '#', '#', cp1251_to_utf8_alloc(display_string));
    eeprom_write_uint16_by_num(EELASTUSEDCELL, selected_index_enc);
    iwdg_setup();

    // Основной цикл программы
    while (1) {
        display_process();   // Обработка отображения
        // Колбек энкодера идёт из SysTick в common.c
        blink_pc14led(1000);
        IWDG->KR = IWDG_REFRESH; // refresh watchdog
    }
}
