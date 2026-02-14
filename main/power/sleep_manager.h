#pragma once

#include "esp_err.h"

/**
 * Initialise le timer d'inactivite pour le deep sleep.
 */
esp_err_t sleep_manager_init(void);

/**
 * Reset le timer d'inactivite (appeler a chaque interaction).
 */
void sleep_manager_reset_timer(void);

/**
 * Entre en deep sleep immediatement.
 * Eteint l'ecran, configure le wakeup GPIO, puis dort.
 */
void sleep_manager_enter_deep_sleep(void);
