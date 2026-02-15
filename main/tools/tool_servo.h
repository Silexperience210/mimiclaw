#pragma once

#include "esp_err.h"
#include <stddef.h>

/* Tools IA pour controler les servos et le capteur ultrason */

esp_err_t tool_move_head_execute(const char *input_json, char *output, size_t output_size);
esp_err_t tool_move_claw_execute(const char *input_json, char *output, size_t output_size);
esp_err_t tool_read_distance_execute(const char *input_json, char *output, size_t output_size);
esp_err_t tool_animate_execute(const char *input_json, char *output, size_t output_size);
