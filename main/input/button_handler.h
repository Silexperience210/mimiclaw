#pragma once

#include "esp_err.h"

/**
 * Initialise les boutons GPIO0 (gauche) et GPIO14 (droite).
 * Lance une task de polling avec debounce.
 */
esp_err_t button_handler_init(void);
