#include "sleep_manager.h"
#include "mimi_config.h"
#include "display/display_ui.h"
#include "display/display_hal.h"

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "esp_sleep.h"

static const char *TAG = "sleep";

static TimerHandle_t s_sleep_timer = NULL;

static void sleep_timer_cb(TimerHandle_t timer)
{
    ESP_LOGI(TAG, "Inactivite detectee, entree en deep sleep...");
    sleep_manager_enter_deep_sleep();
}

esp_err_t sleep_manager_init(void)
{
    s_sleep_timer = xTimerCreate(
        "sleep_tmr",
        pdMS_TO_TICKS(MIMI_SLEEP_TIMEOUT_MS),
        pdFALSE,  /* one-shot */
        NULL,
        sleep_timer_cb);

    if (!s_sleep_timer) return ESP_ERR_NO_MEM;

    xTimerStart(s_sleep_timer, 0);

    ESP_LOGI(TAG, "Sleep manager init (timeout=%d min)",
             MIMI_SLEEP_TIMEOUT_MS / 60000);
    return ESP_OK;
}

void sleep_manager_reset_timer(void)
{
    if (s_sleep_timer) {
        xTimerReset(s_sleep_timer, 0);
    }
}

void sleep_manager_enter_deep_sleep(void)
{
    ESP_LOGI(TAG, "Entering deep sleep, wakeup on GPIO%d", MIMI_SLEEP_WAKEUP_PIN);

    /* Eteindre l'ecran */
    display_ui_set_state(DISPLAY_SLEEP);
    display_hal_sleep();

    /* Petit delai pour que les logs sortent */
    vTaskDelay(pdMS_TO_TICKS(100));

    /* Configurer le reveil par GPIO14 (bouton droit, active low) */
    esp_sleep_enable_ext0_wakeup(MIMI_SLEEP_WAKEUP_PIN, 0);

    /* Deep sleep */
    esp_deep_sleep_start();
    /* Ne revient jamais ici — le CPU reboot au reveil */
}
