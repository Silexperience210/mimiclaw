#pragma once

#include "esp_err.h"
#include <stddef.h>

/* Tools IA pour les mises a jour firmware */

esp_err_t tool_check_update_execute(const char *input_json, char *output, size_t output_size);
esp_err_t tool_do_update_execute(const char *input_json, char *output, size_t output_size);
