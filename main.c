
#define F_CPU 3333333UL

#include <avr/io.h>
#include <util/delay.h>
#include <stdbool.h>
#include <stdint.h>

// Живлення VRX1–VRX4: PC0–PC3 (активний HIGH)
#define VRX_PINS (PIN0_bm | PIN1_bm | PIN2_bm | PIN3_bm)

// Кнопка керування: PB1 (активний LOW)
#define BUTTON_PIN PIN1_bm

// ADG704: PA4=A0, PA5=A1, PA6=EN
#define MUX_A0 PIN4_bm
#define MUX_A1 PIN5_bm
#define MUX_EN PIN6_bm

#define DEBOUNCE_MS 30
#define LONG_PRESS_MS 2000
#define VRX_STARTUP_MS 500

static uint8_t channel = 0;
static bool system_on = true;

// Вимкнути відеокомутатор (EN = LOW)
static void video_disable(void)
{
    PORTA.OUTCLR = MUX_EN;
}

// Вибрати відеоканал і ввімкнути комутатор
static void video_select(uint8_t ch)
{
    // Вимикаємо комутатор перед зміною адреси
    video_disable();

    // Спочатку скидаємо A0 та A1
    PORTA.OUTCLR = MUX_A0 | MUX_A1;

    // Встановлюємо адресу каналу
    if (ch & 0x01)
    {
        PORTA.OUTSET = MUX_A0;
    }

    if (ch & 0x02)
    {
        PORTA.OUTSET = MUX_A1;
    }

    // EN = HIGH: дозволяємо проходження відео
    PORTA.OUTSET = MUX_EN;
}

// Оновлення живлення VRX та вибору відеоканалу
static void update_outputs(void)
{
    // Вимикаємо відео перед перемиканням VRX
    video_disable();

    // Вимикаємо всі VRX
    PORTC.OUTCLR = VRX_PINS;

    // Якщо система вимкнена, нічого не запускаємо
    if (!system_on)
    {
        return;
    }

    // Увімкнути тільки вибраний VRX
    PORTC.OUTSET = (uint8_t)(1U << channel);

    // Очікуємо стабілізації відеоприймача
    _delay_ms(VRX_STARTUP_MS);

    // Підключаємо відповідний відеоканал
    video_select(channel);
}

int main(void)
{
    // Початковий стан: усі VRX вимкнені
    PORTC.OUTCLR = VRX_PINS;
    PORTC.DIRSET = VRX_PINS;

    // Початковий стан ADG704: вимкнений
    PORTA.OUTCLR = MUX_EN | MUX_A0 | MUX_A1;
    PORTA.DIRSET = MUX_A0 | MUX_A1 | MUX_EN;

    // Кнопка PB1 як вхід із підтягуванням
    PORTB.DIRCLR = BUTTON_PIN;
    PORTB.PIN1CTRL = PORT_PULLUPEN_bm;

    // Запускаємо VRX1
    update_outputs();

    bool last_raw = false;
    bool stable_pressed = false;
    bool long_handled = false;

    uint16_t debounce_time = 0;
    uint16_t press_time = 0;

    while (1)
    {
        // Натиснута кнопка = LOW
        bool raw_pressed = !(PORTB.IN & BUTTON_PIN);

        // Антидребезг кнопки
        if (raw_pressed != last_raw)
        {
            debounce_time = 0;
            last_raw = raw_pressed;
        }
        else if (debounce_time < DEBOUNCE_MS)
        {
            debounce_time++;
        }

        // Обробка зміни стабільного стану кнопки
        if (debounce_time >= DEBOUNCE_MS &&
            raw_pressed != stable_pressed)
        {
            stable_pressed = raw_pressed;

            if (stable_pressed)
            {
                press_time = 0;
                long_handled = false;
            }
            else if (!long_handled && system_on)
            {
                // Коротке натискання: наступний канал
                channel = (channel + 1) % 4;
                update_outputs();
            }
        }

        // Довге натискання: увімкнення/вимкнення
        if (stable_pressed && !long_handled)
        {
            if (press_time < LONG_PRESS_MS)
            {
                press_time++;
            }

            if (press_time >= LONG_PRESS_MS)
            {
                system_on = !system_on;
                update_outputs();
                long_handled = true;
            }
        }

        _delay_ms(1);
    }
}
