#pragma once

#include "esp_err.h"
#include <stddef.h>

/* Tools IA pour la perception spatiale : radar, sentinelle, scan de piece */

esp_err_t tool_radar_scan_execute(const char *input_json, char *output, size_t output_size);
esp_err_t tool_sentinel_mode_execute(const char *input_json, char *output, size_t output_size);
esp_err_t tool_get_room_scan_execute(const char *input_json, char *output, size_t output_size);

/* Met a jour le chat cible pour les alertes sentinelle */
void tool_perception_set_chat(const char *channel, const char *chat_id);
