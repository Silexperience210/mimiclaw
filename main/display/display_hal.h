#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * Initialise le bus I80, le panel ST7789 et le backlight.
 */
esp_err_t display_hal_init(void);

/**
 * Pousse un buffer RGB565 vers l'ecran.
 * @param x, y  coin superieur gauche
 * @param w, h  dimensions de la zone
 * @param data  buffer RGB565 (w * h * 2 octets)
 */
esp_err_t display_hal_flush(int x, int y, int w, int h, const uint16_t *data);

/**
 * Controle du backlight (on/off).
 */
void display_hal_backlight(bool on);

/**
 * Envoie la commande SLEEP IN au ST7789 (economie energie).
 */
void display_hal_sleep(void);

/**
 * Envoie la commande SLEEP OUT au ST7789 (reveil).
 */
void display_hal_wake(void);
