#include "tool_perception.h"
#include "hardware/sonar_radar.h"
#include "input/gesture_detect.h"
#include "hardware/body_animator.h"
#include "display/display_ui.h"

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "cJSON.h"

static const char *TAG = "tool_percep";

/* Stocke le dernier canal/chat_id pour les alertes sentinelle */
static char s_last_channel[16] = "telegram";
static char s_last_chat_id[32] = {0};

void tool_perception_set_chat(const char *channel, const char *chat_id)
{
    if (channel) strncpy(s_last_channel, channel, sizeof(s_last_channel) - 1);
    if (chat_id) strncpy(s_last_chat_id, chat_id, sizeof(s_last_chat_id) - 1);
}

esp_err_t tool_radar_scan_execute(const char *input_json, char *output, size_t output_size)
{
    cJSON *root = cJSON_Parse(input_json);
    if (!root) {
        snprintf(output, output_size, "Error: invalid JSON");
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *action = cJSON_GetObjectItem(root, "action");
    const char *act = (action && cJSON_IsString(action)) ? action->valuestring : "start";

    if (strcmp(act, "start") == 0) {
        sonar_radar_set_mode(RADAR_SCAN);
        display_ui_set_state(DISPLAY_RADAR);
        body_animator_set_state(DISPLAY_RADAR);
        snprintf(output, output_size,
                 "Radar scan active! Balayage %d-%d deg. "
                 "L'ecran affiche la carte sonar en temps reel.",
                 RADAR_SWEEP_MIN, RADAR_SWEEP_MAX);
    } else if (strcmp(act, "stop") == 0) {
        sonar_radar_set_mode(RADAR_OFF);
        display_ui_set_state(DISPLAY_IDLE);
        body_animator_set_state(DISPLAY_IDLE);
        snprintf(output, output_size, "Radar scan desactive.");
    } else {
        snprintf(output, output_size, "Error: action must be 'start' or 'stop'");
    }

    cJSON_Delete(root);
    ESP_LOGI(TAG, "radar_scan: %s", output);
    return ESP_OK;
}

esp_err_t tool_sentinel_mode_execute(const char *input_json, char *output, size_t output_size)
{
    cJSON *root = cJSON_Parse(input_json);
    if (!root) {
        snprintf(output, output_size, "Error: invalid JSON");
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *action = cJSON_GetObjectItem(root, "action");
    const char *act = (action && cJSON_IsString(action)) ? action->valuestring : "arm";

    if (strcmp(act, "arm") == 0) {
        /* D'abord faire un scan de reference si pas encore fait */
        if (sonar_radar_get_mode() == RADAR_OFF) {
            sonar_radar_set_mode(RADAR_SCAN);
        }
        /* Sauvegarder la baseline apres un moment (le scan est en cours) */
        sonar_radar_save_baseline();
        sonar_radar_set_mode(RADAR_SENTINEL);
        display_ui_set_state(DISPLAY_RADAR);
        body_animator_set_state(DISPLAY_RADAR);

        /* Configurer l'alerte Telegram */
        if (s_last_chat_id[0]) {
            sonar_radar_set_alert_target(s_last_channel, s_last_chat_id);
        }

        snprintf(output, output_size,
                 "Mode sentinelle ARME! Baseline sauvegardee. "
                 "Je surveille et j'envoie une alerte si quelque chose change.");
    } else if (strcmp(act, "disarm") == 0) {
        sonar_radar_set_mode(RADAR_OFF);
        display_ui_set_state(DISPLAY_IDLE);
        body_animator_set_state(DISPLAY_IDLE);
        snprintf(output, output_size, "Mode sentinelle desactive.");
    } else {
        snprintf(output, output_size, "Error: action must be 'arm' or 'disarm'");
    }

    cJSON_Delete(root);
    ESP_LOGI(TAG, "sentinel_mode: %s", output);
    return ESP_OK;
}

esp_err_t tool_get_room_scan_execute(const char *input_json, char *output, size_t output_size)
{
    (void)input_json;
    sonar_radar_build_report(output, output_size);
    ESP_LOGI(TAG, "get_room_scan: %d bytes", (int)strlen(output));
    return ESP_OK;
}
