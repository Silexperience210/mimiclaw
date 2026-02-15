#include "servo_driver.h"
#include "mimi_config.h"

#include "driver/ledc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "servo";

/* Config par servo : GPIO + canal LEDC */
static const struct {
    int gpio;
    ledc_channel_t channel;
} s_servo_cfg[SERVO_COUNT] = {
    [SERVO_HEAD_H] = { MIMI_SERVO_HEAD_H_PIN, LEDC_CHANNEL_0 },
    [SERVO_HEAD_V] = { MIMI_SERVO_HEAD_V_PIN, LEDC_CHANNEL_1 },
    [SERVO_CLAW_L] = { MIMI_SERVO_CLAW_L_PIN, LEDC_CHANNEL_2 },
    [SERVO_CLAW_R] = { MIMI_SERVO_CLAW_R_PIN, LEDC_CHANNEL_3 },
};

/* Angle actuel de chaque servo */
static uint8_t s_current_angle[SERVO_COUNT];

/* Convertit un angle (0-180) en duty LEDC (resolution 14 bits, 50Hz) */
static uint32_t angle_to_duty(uint8_t angle)
{
    /* 50Hz → periode 20ms = 20000us
     * Resolution 14 bits → max = 16383
     * duty = pulse_us / 20000 * 16383 */
    uint32_t pulse_us = MIMI_SERVO_MIN_US +
        (uint32_t)(MIMI_SERVO_MAX_US - MIMI_SERVO_MIN_US) * angle / 180;
    return pulse_us * 16383 / 20000;
}

esp_err_t servo_driver_init(void)
{
    /* Timer LEDC partage pour tous les servos */
    ledc_timer_config_t timer_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num  = LEDC_TIMER_1,
        .duty_resolution = LEDC_TIMER_14_BIT,
        .freq_hz    = MIMI_SERVO_FREQ_HZ,
        .clk_cfg    = LEDC_AUTO_CLK,
    };
    esp_err_t ret = ledc_timer_config(&timer_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LEDC timer config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Angles initiaux : tete centre, pinces fermees */
    const uint8_t init_angles[SERVO_COUNT] = { 90, 90, 0, 0 };

    for (int i = 0; i < SERVO_COUNT; i++) {
        s_current_angle[i] = init_angles[i];

        ledc_channel_config_t ch_cfg = {
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel    = s_servo_cfg[i].channel,
            .timer_sel  = LEDC_TIMER_1,
            .intr_type  = LEDC_INTR_DISABLE,
            .gpio_num   = s_servo_cfg[i].gpio,
            .duty       = angle_to_duty(init_angles[i]),
            .hpoint     = 0,
        };
        ret = ledc_channel_config(&ch_cfg);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "LEDC channel %d config failed", i);
            return ret;
        }
    }

    ESP_LOGI(TAG, "Servo driver init OK (4 servos, 50Hz PWM)");
    return ESP_OK;
}

esp_err_t servo_set_angle_immediate(servo_id_t id, uint8_t angle)
{
    if (id >= SERVO_COUNT) return ESP_ERR_INVALID_ARG;
    if (angle > 180) angle = 180;

    uint32_t duty = angle_to_duty(angle);
    esp_err_t ret = ledc_set_duty(LEDC_LOW_SPEED_MODE, s_servo_cfg[id].channel, duty);
    if (ret == ESP_OK) {
        ret = ledc_update_duty(LEDC_LOW_SPEED_MODE, s_servo_cfg[id].channel);
    }
    if (ret == ESP_OK) {
        s_current_angle[id] = angle;
    }
    return ret;
}

esp_err_t servo_set_angle(servo_id_t id, uint8_t angle)
{
    if (id >= SERVO_COUNT) return ESP_ERR_INVALID_ARG;
    if (angle > 180) angle = 180;

    int current = s_current_angle[id];
    int target  = angle;
    int step    = (target > current) ? MIMI_SERVO_STEP_DEG : -MIMI_SERVO_STEP_DEG;

    while (current != target) {
        current += step;
        /* Clamp au target */
        if ((step > 0 && current > target) || (step < 0 && current < target)) {
            current = target;
        }
        servo_set_angle_immediate(id, (uint8_t)current);
        vTaskDelay(pdMS_TO_TICKS(MIMI_SERVO_STEP_MS));
    }

    return ESP_OK;
}

uint8_t servo_get_angle(servo_id_t id)
{
    if (id >= SERVO_COUNT) return 0;
    return s_current_angle[id];
}
