#pragma once

#include "esp_err.h"
#include <stdint.h>

/* Identifiants des 4 servos */
typedef enum {
    SERVO_HEAD_H = 0,  /* tete horizontal (gauche/droite) */
    SERVO_HEAD_V = 1,  /* tete vertical (haut/bas) */
    SERVO_CLAW_L = 2,  /* pince gauche */
    SERVO_CLAW_R = 3,  /* pince droite */
    SERVO_COUNT  = 4,
} servo_id_t;

/**
 * Initialise les 4 canaux LEDC PWM pour les servos.
 * Position boot : tete centre (90°), pinces fermees (0°).
 */
esp_err_t servo_driver_init(void);

/**
 * Deplace un servo vers l'angle cible de maniere progressive.
 * @param id    Identifiant du servo
 * @param angle Angle cible (0-180)
 */
esp_err_t servo_set_angle(servo_id_t id, uint8_t angle);

/**
 * Deplace un servo immediatement (sans interpolation).
 */
esp_err_t servo_set_angle_immediate(servo_id_t id, uint8_t angle);

/**
 * Retourne l'angle actuel d'un servo.
 */
uint8_t servo_get_angle(servo_id_t id);
