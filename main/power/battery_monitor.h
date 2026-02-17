#pragma once

#include "esp_err.h"
#include <stdbool.h>

/**
 * Initialise l'ADC pour lire la tension batterie (GPIO4).
 * Lance une task de polling periodique.
 */
esp_err_t battery_monitor_init(void);

/**
 * Retourne true si la batterie est en charge (USB connecte).
 */
bool battery_is_charging(void);

/**
 * Retourne le pourcentage de charge (0-100).
 */
int battery_get_percent(void);

/**
 * Retourne la tension batterie en mV.
 */
int battery_get_voltage_mv(void);
