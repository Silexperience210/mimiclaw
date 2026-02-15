#include "ultrasonic.h"
#include "mimi_config.h"

#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdlib.h>

static const char *TAG = "ultrasonic";

esp_err_t ultrasonic_init(void)
{
    /* TRIG = sortie */
    gpio_config_t trig_cfg = {
        .pin_bit_mask = (1ULL << MIMI_US_TRIG_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t ret = gpio_config(&trig_cfg);
    if (ret != ESP_OK) return ret;

    /* ECHO = entree */
    gpio_config_t echo_cfg = {
        .pin_bit_mask = (1ULL << MIMI_US_ECHO_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ret = gpio_config(&echo_cfg);
    if (ret != ESP_OK) return ret;

    gpio_set_level(MIMI_US_TRIG_PIN, 0);

    ESP_LOGI(TAG, "HC-SR04 init OK (TRIG=%d, ECHO=%d)",
             MIMI_US_TRIG_PIN, MIMI_US_ECHO_PIN);
    return ESP_OK;
}

/* Lecture unique sans filtre — retourne cm ou -1 */
static int single_read_cm(void)
{
    /* Pulse TRIG 10us */
    gpio_set_level(MIMI_US_TRIG_PIN, 0);
    esp_rom_delay_us(2);
    gpio_set_level(MIMI_US_TRIG_PIN, 1);
    esp_rom_delay_us(10);
    gpio_set_level(MIMI_US_TRIG_PIN, 0);

    /* Attendre ECHO monte (timeout) */
    int64_t start = esp_timer_get_time();
    while (gpio_get_level(MIMI_US_ECHO_PIN) == 0) {
        if ((esp_timer_get_time() - start) > MIMI_US_TIMEOUT_US) return -1;
    }

    /* Mesurer duree ECHO haut */
    int64_t echo_start = esp_timer_get_time();
    while (gpio_get_level(MIMI_US_ECHO_PIN) == 1) {
        if ((esp_timer_get_time() - echo_start) > MIMI_US_TIMEOUT_US) return -1;
    }
    int64_t echo_end = esp_timer_get_time();

    /* Distance = duree_us / 58 (aller-retour, vitesse du son) */
    int duration_us = (int)(echo_end - echo_start);
    return duration_us / 58;
}

/* Comparateur pour qsort */
static int cmp_int(const void *a, const void *b)
{
    return (*(const int *)a) - (*(const int *)b);
}

int ultrasonic_read_cm(void)
{
    int samples[MIMI_US_MEDIAN_SAMPLES];
    int valid = 0;

    for (int i = 0; i < MIMI_US_MEDIAN_SAMPLES; i++) {
        int d = single_read_cm();
        if (d > 0) {
            samples[valid++] = d;
        }
        if (i < MIMI_US_MEDIAN_SAMPLES - 1) {
            esp_rom_delay_us(1000); /* 1ms entre lectures */
        }
    }

    if (valid == 0) return -1;

    /* Mediane */
    qsort(samples, valid, sizeof(int), cmp_int);
    return samples[valid / 2];
}
