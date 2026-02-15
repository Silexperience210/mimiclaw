#pragma once

#include "esp_err.h"

/**
 * Initialise les GPIO du HC-SR04.
 */
esp_err_t ultrasonic_init(void);

/**
 * Lit la distance en cm (filtre mediane sur 3 lectures).
 * @return distance en cm, -1 si erreur/timeout
 */
int ultrasonic_read_cm(void);
