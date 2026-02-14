#pragma once

#include "esp_err.h"
#include <stdbool.h>

/* Etats de l'ecran */
typedef enum {
    DISPLAY_IDLE,       /* Lobster anime + status */
    DISPLAY_THINKING,   /* Animation "reflexion" */
    DISPLAY_MESSAGE,    /* Affiche dernier message */
    DISPLAY_PORTAL,     /* Info portail captif */
    DISPLAY_SLEEP,      /* Ecran eteint */
} display_state_t;

/**
 * Initialise l'UI et lance la task d'affichage.
 */
esp_err_t display_ui_init(void);

/**
 * Change l'etat de l'ecran.
 */
void display_ui_set_state(display_state_t state);

/**
 * Met a jour le message affiche (copie interne).
 */
void display_ui_set_message(const char *text);

/**
 * Met a jour le status WiFi.
 */
void display_ui_set_status(bool wifi_ok, const char *ip);

/**
 * Retourne l'etat courant.
 */
display_state_t display_ui_get_state(void);

/**
 * Passe a l'ecran suivant (cycle).
 */
void display_ui_next_screen(void);
