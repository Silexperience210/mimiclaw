#pragma once

#include "esp_err.h"
#include "display/display_ui.h"
#include "input/gesture_detect.h"

/**
 * Initialise la task FreeRTOS d'animation corporelle.
 * Doit etre appele apres servo_driver_init() et ultrasonic_init().
 */
esp_err_t body_animator_init(void);

/**
 * Met a jour l'etat pour choisir l'animation corporelle.
 * Appele depuis agent_loop quand l'etat change.
 */
void body_animator_set_state(display_state_t state);

/**
 * Met a jour l'humeur pour les animations corporelles.
 */
void body_animator_set_mood(lobster_mood_t mood);

/**
 * Place les servos en position repos (avant deep sleep).
 */
void body_animator_sleep(void);

/**
 * Joue une animation nommee (wave, nod_yes, nod_no, celebrate, think, sleep).
 */
void body_animator_play(const char *name);

/**
 * Retourne la derniere distance lue par le capteur ultrason (cm, -1 si erreur).
 */
int body_animator_get_distance(void);

/**
 * Retourne le dernier geste detecte.
 */
gesture_t body_animator_get_last_gesture(void);

/**
 * Construit la chaine de perception complete pour le context_builder.
 */
void body_animator_build_perception(char *buf, size_t size);
