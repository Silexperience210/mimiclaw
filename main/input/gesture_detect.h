#pragma once

#include "esp_err.h"
#include <stdbool.h>

/* Types de gestes reconnus */
typedef enum {
    GESTURE_NONE,       /* pas de geste */
    GESTURE_WAVE,       /* main qui agite rapidement */
    GESTURE_PUSH,       /* main qui approche puis recule */
    GESTURE_HOLD,       /* main stable et proche (< 30cm) */
    GESTURE_SWIPE,      /* passage rapide devant le capteur */
} gesture_t;

/**
 * Initialise le detecteur de gestes.
 */
esp_err_t gesture_detect_init(void);

/**
 * Met a jour avec une nouvelle mesure de distance.
 * Appele par body_animator a chaque tick (~200ms).
 * @param distance  distance en cm, -1 si erreur
 */
void gesture_detect_update(int distance);

/**
 * Recupere le dernier geste detecte et le reset.
 * @return  geste detecte, GESTURE_NONE si aucun
 */
gesture_t gesture_detect_poll(void);

/**
 * Peek le geste actuel sans le consommer.
 */
gesture_t gesture_detect_peek(void);

/**
 * Nom du geste en texte.
 */
const char *gesture_detect_name(gesture_t g);

/**
 * Retourne true si une main est proche (< 30cm, stable).
 * Utile pour l'etch-a-sketch.
 */
bool gesture_detect_is_hand_close(void);

/**
 * Retourne la distance moyenne recente (derniers 5 samples).
 * -1 si pas de donnees valides.
 */
int gesture_detect_avg_distance(void);
