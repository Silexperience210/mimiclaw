#include "sleep_manager.h"
#include "mimi_config.h"
#include "display/display_ui.h"
#include "display/display_hal.h"
#ifdef MIMI_HAS_SERVOS
#include "hardware/body_animator.h"
#endif

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "esp_sleep.h"

static const char *TAG = "sleep";

static TimerHandle_t s_sleep_timer = NULL;
static volatile bool s_deep_sleep_requested = false;
static const char *s_sleep_reason = NULL;

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

#ifdef MIMI_HAS_SERVOS
    /* Servos en position repos */
    body_animator_sleep();
#endif

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

void sleep_manager_request_deep_sleep(const char *reason)
{
    if (s_deep_sleep_requested) return;  /* Deja demande */
    
    s_deep_sleep_requested = true;
    s_sleep_reason = reason ? reason : "unknown";
    
    ESP_LOGW(TAG, "Deep sleep requested (reason: %s), entering in 5s...", s_sleep_reason);
    
    /* Delai de grace de 5 secondes avant la mise en veille - non bloquant */
    /* Utilise un timer pour ne pas bloquer la task appelante */
    TimerHandle_t delay_timer = xTimerCreate(
        "sleep_delay", pdMS_TO_TICKS(5000), pdFALSE, NULL,
        (TimerCallbackFunction_t)sleep_manager_enter_deep_sleep);
    
    if (delay_timer) {
        xTimerStart(delay_timer, 0);
    } else {
        /* Fallback: sleep immediat si timer creation echoue */
        sleep_manager_enter_deep_sleep();
    }
}
