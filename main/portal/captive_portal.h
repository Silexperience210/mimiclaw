#pragma once

#include "esp_err.h"
#include <stdbool.h>

/**
 * Demarre le portail captif : SoftAP + DNS captif + serveur HTTP config.
 * Accessible sur 192.168.4.1:80
 */
esp_err_t captive_portal_start(void);

/**
 * Arrete le portail captif (AP + DNS + HTTP).
 */
esp_err_t captive_portal_stop(void);

/**
 * Retourne true si le portail est actif.
 */
bool captive_portal_is_active(void);
