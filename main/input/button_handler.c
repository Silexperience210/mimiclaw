#include "button_handler.h"
#include "mimi_config.h"
#include "display/display_ui.h"
#include "display/display_hal.h"
#include "power/sleep_manager.h"
#include "portal/captive_portal.h"

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "buttons";

/* Etat de chaque bouton */
typedef struct {
    int gpio;
    bool pressed;
    int64_t press_start;  /* timestamp en us */
} btn_state_t;

static btn_state_t s_btn_left  = { .gpio = MIMI_BTN_LEFT };
static btn_state_t s_btn_right = { .gpio = MIMI_BTN_RIGHT };

static bool read_btn(btn_state_t *btn)
{
    return gpio_get_level(btn->gpio) == 0;  /* active low */
}

static void handle_short_press_left(void)
{
    ESP_LOGI(TAG, "Bouton gauche : ecran suivant");
    sleep_manager_reset_timer();
    display_ui_next_screen();
}

static void handle_long_press_left(void)
{
    ESP_LOGI(TAG, "Bouton gauche long : toggle portail");
    sleep_manager_reset_timer();
    if (captive_portal_is_active()) {
        captive_portal_stop();
        display_ui_set_state(DISPLAY_IDLE);
    } else {
        captive_portal_start();
        display_ui_set_state(DISPLAY_PORTAL);
    }
}

static void handle_short_press_right(void)
{
    ESP_LOGI(TAG, "Bouton droit : toggle backlight");
    sleep_manager_reset_timer();
    /* Toggle backlight */
    static bool bl_on = true;
    bl_on = !bl_on;
    display_hal_backlight(bl_on);
}

static void handle_long_press_right(void)
{
    ESP_LOGI(TAG, "Bouton droit long : deep sleep");
    sleep_manager_enter_deep_sleep();
}

static void update_btn(btn_state_t *btn,
                       void (*on_short)(void),
                       void (*on_long)(void))
{
    bool now_pressed = read_btn(btn);

    if (now_pressed && !btn->pressed) {
        /* Debut appui */
        btn->pressed = true;
        btn->press_start = esp_timer_get_time();
    } else if (!now_pressed && btn->pressed) {
        /* Relache */
        btn->pressed = false;
        int64_t duration_ms = (esp_timer_get_time() - btn->press_start) / 1000;

        if (duration_ms >= MIMI_BTN_LONG_MS) {
            on_long();
        } else if (duration_ms >= MIMI_BTN_DEBOUNCE_MS) {
            on_short();
        }
    }
}

static void button_task(void *arg)
{
    ESP_LOGI(TAG, "Button handler started");

    while (1) {
        update_btn(&s_btn_left, handle_short_press_left, handle_long_press_left);
        update_btn(&s_btn_right, handle_short_press_right, handle_long_press_right);
        vTaskDelay(pdMS_TO_TICKS(20));  /* polling a 50Hz */
    }
}

esp_err_t button_handler_init(void)
{
    /* Config GPIO en input avec pullup */
    gpio_config_t io_cfg = {
        .pin_bit_mask = (1ULL << MIMI_BTN_LEFT) | (1ULL << MIMI_BTN_RIGHT),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_cfg);

    xTaskCreatePinnedToCore(
        button_task, "buttons",
        MIMI_BTN_STACK, NULL,
        MIMI_BTN_PRIO, NULL, MIMI_BTN_CORE);

    ESP_LOGI(TAG, "Buttons initialized (GPIO%d, GPIO%d)", MIMI_BTN_LEFT, MIMI_BTN_RIGHT);
    return ESP_OK;
}
