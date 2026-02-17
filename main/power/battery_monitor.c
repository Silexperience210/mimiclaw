#include "battery_monitor.h"
#include "mimi_config.h"
#include "display/display_ui.h"

#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "battery";

static adc_oneshot_unit_handle_t s_adc_handle = NULL;
static adc_cali_handle_t s_cali_handle = NULL;

static int s_voltage_mv = 0;       /* tension batterie actuelle (mV) */
static int s_percent = 0;          /* pourcentage 0-100 */
static bool s_charging = false;    /* en charge ? */

/* Historique pour moyenne glissante */
static int s_samples[MIMI_BATT_AVG_SAMPLES];
static int s_sample_idx = 0;
static int s_sample_count = 0;

/* Detection tendance : tension monte = en charge */
static int s_prev_avg_mv = 0;
static int s_rising_count = 0;

/* --- Helpers --- */

static int voltage_to_percent(int mv)
{
    if (mv >= MIMI_BATT_FULL_MV) return 100;
    if (mv <= MIMI_BATT_EMPTY_MV) return 0;

    /* Courbe lineaire simplifiee */
    return (mv - MIMI_BATT_EMPTY_MV) * 100 / (MIMI_BATT_FULL_MV - MIMI_BATT_EMPTY_MV);
}

static int compute_average(void)
{
    if (s_sample_count == 0) return 0;
    int sum = 0;
    int n = (s_sample_count < MIMI_BATT_AVG_SAMPLES) ? s_sample_count : MIMI_BATT_AVG_SAMPLES;
    for (int i = 0; i < n; i++) {
        sum += s_samples[i];
    }
    return sum / n;
}

static void battery_read(void)
{
    int raw = 0;
    esp_err_t ret = adc_oneshot_read(s_adc_handle, MIMI_BATT_ADC_CHANNEL, &raw);
    if (ret != ESP_OK) return;

    /* Calibration → tension en mV (cote ADC) */
    int adc_mv = 0;
    if (s_cali_handle) {
        adc_cali_raw_to_voltage(s_cali_handle, raw, &adc_mv);
    } else {
        /* Fallback sans calibration : estimation pour 12-bit, Vref ~1100mV, atten 11dB */
        adc_mv = raw * 2500 / 4095;
    }

    /* Appliquer le facteur du pont diviseur */
    int batt_mv = (int)(adc_mv * MIMI_BATT_DIVIDER_RATIO);

    /* Ajouter a la moyenne glissante */
    s_samples[s_sample_idx] = batt_mv;
    s_sample_idx = (s_sample_idx + 1) % MIMI_BATT_AVG_SAMPLES;
    if (s_sample_count < MIMI_BATT_AVG_SAMPLES) s_sample_count++;

    int avg_mv = compute_average();
    s_voltage_mv = avg_mv;
    s_percent = voltage_to_percent(avg_mv);

    /* Detection charge : tension haute OU tendance a la hausse */
    bool was_charging = s_charging;

    if (avg_mv >= MIMI_BATT_CHARGE_MV) {
        /* Tension tres haute = USB connecte */
        s_charging = true;
        s_rising_count = 0;
    } else if (s_prev_avg_mv > 0 && avg_mv > s_prev_avg_mv + 5) {
        /* Tension monte de >5mV = probablement en charge */
        s_rising_count++;
        if (s_rising_count >= 3) {
            s_charging = true;
        }
    } else if (s_prev_avg_mv > 0 && avg_mv < s_prev_avg_mv - 10) {
        /* Tension descend = decharge */
        s_rising_count = 0;
        s_charging = false;
    }

    s_prev_avg_mv = avg_mv;

    /* Log si changement d'etat */
    if (s_charging != was_charging) {
        ESP_LOGI(TAG, "Charge %s (V=%dmV, %d%%)",
                 s_charging ? "DETECTEE" : "TERMINEE", avg_mv, s_percent);
    }
}

/* --- Task de polling --- */

static void battery_task(void *arg)
{
    ESP_LOGI(TAG, "Battery monitor started (poll=%dms)", MIMI_BATT_POLL_MS);

    /* Premieres lectures pour remplir le buffer */
    for (int i = 0; i < MIMI_BATT_AVG_SAMPLES; i++) {
        battery_read();
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    ESP_LOGI(TAG, "Batterie: %dmV (%d%%) %s",
             s_voltage_mv, s_percent, s_charging ? "[CHARGE]" : "[DECHARGE]");

    while (1) {
        battery_read();

        /* Basculer l'affichage en mode charge si necessaire */
        display_state_t cur = display_ui_get_state();
        if (s_charging && cur != DISPLAY_CHARGING && cur != DISPLAY_PORTAL
                       && cur != DISPLAY_SLEEP) {
            display_ui_set_state(DISPLAY_CHARGING);
        } else if (!s_charging && cur == DISPLAY_CHARGING) {
            display_ui_set_state(DISPLAY_IDLE);
        }

        vTaskDelay(pdMS_TO_TICKS(MIMI_BATT_POLL_MS));
    }
}

/* --- API publique --- */

esp_err_t battery_monitor_init(void)
{
    /* Config ADC oneshot */
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = ADC_UNIT_1,
    };
    esp_err_t ret = adc_oneshot_new_unit(&unit_cfg, &s_adc_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ADC unit init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ret = adc_oneshot_config_channel(s_adc_handle, MIMI_BATT_ADC_CHANNEL, &chan_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ADC channel config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Calibration (curve fitting sur ESP32-S3) */
    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id = ADC_UNIT_1,
        .chan = MIMI_BATT_ADC_CHANNEL,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ret = adc_cali_create_scheme_curve_fitting(&cali_cfg, &s_cali_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "ADC calibration non dispo, fallback estimation");
        s_cali_handle = NULL;
    }

    /* Task polling */
    BaseType_t xret = xTaskCreatePinnedToCore(
        battery_task, "batt_mon",
        3 * 1024, NULL,
        2, NULL, 0);

    if (xret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create battery task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Battery monitor init OK (GPIO%d, ADC1_CH%d)",
             MIMI_BATT_ADC_PIN, MIMI_BATT_ADC_CHANNEL);
    return ESP_OK;
}

bool battery_is_charging(void)
{
    return s_charging;
}

int battery_get_percent(void)
{
    return s_percent;
}

int battery_get_voltage_mv(void)
{
    return s_voltage_mv;
}
