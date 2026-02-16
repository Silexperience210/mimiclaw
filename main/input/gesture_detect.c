#include "gesture_detect.h"

#include <string.h>
#include "esp_log.h"

static const char *TAG = "gesture";

#define HIST_SIZE  20   /* 20 samples = 4 secondes a 200ms */

static int s_history[HIST_SIZE];
static int s_idx = 0;
static int s_count = 0;       /* nombre total de samples recus */
static gesture_t s_gesture = GESTURE_NONE;
static int s_hold_count = 0;  /* ticks consecutifs en hold (< 30cm stable) */

/* Anti-rebond : cooldown apres detection d'un geste */
static int s_cooldown = 0;
#define GESTURE_COOLDOWN  10  /* ~2 secondes */

/* Seuils */
#define WAVE_MIN_CHANGES    4   /* changements de direction minimum */
#define WAVE_MIN_AMPLITUDE  12  /* cm d'amplitude minimum */
#define PUSH_DROP_CM        25  /* chute de distance pour push */
#define PUSH_RISE_CM        25  /* remontee de distance pour push */
#define HOLD_DIST_CM        30  /* distance max pour hold */
#define HOLD_VARIANCE_CM    8   /* variance max pour hold */
#define HOLD_MIN_TICKS      10  /* 2 secondes minimum */
#define SWIPE_CLOSE_CM      35  /* distance de passage */
#define SWIPE_MAX_TICKS     6   /* duree max du passage (~1.2s) */

esp_err_t gesture_detect_init(void)
{
    memset(s_history, 0xFF, sizeof(s_history)); /* -1 partout */
    s_idx = 0;
    s_count = 0;
    s_gesture = GESTURE_NONE;
    s_hold_count = 0;
    s_cooldown = 0;

    ESP_LOGI(TAG, "Gesture detect init OK (buffer=%d samples)", HIST_SIZE);
    return ESP_OK;
}

/* Retourne la valeur dans l'historique, index 0 = plus recent */
static int hist(int ago)
{
    int i = (s_idx - 1 - ago + HIST_SIZE * 2) % HIST_SIZE;
    return s_history[i];
}

/* Detecte le geste WAVE : oscillations rapides */
static bool detect_wave(void)
{
    if (s_count < 10) return false;

    int changes = 0;
    int max_d = 0, min_d = 9999;
    int prev_dir = 0;

    for (int i = 0; i < 12 && i < s_count; i++) {
        int d = hist(i);
        int d_next = hist(i + 1);
        if (d < 0 || d_next < 0) continue;

        if (d > max_d) max_d = d;
        if (d < min_d) min_d = d;

        int dir = (d > d_next) ? 1 : (d < d_next) ? -1 : 0;
        if (dir != 0 && dir != prev_dir && prev_dir != 0) {
            changes++;
        }
        if (dir != 0) prev_dir = dir;
    }

    return (changes >= WAVE_MIN_CHANGES && (max_d - min_d) >= WAVE_MIN_AMPLITUDE);
}

/* Detecte le geste PUSH : approche rapide puis recul */
static bool detect_push(void)
{
    if (s_count < 8) return false;

    /* Cherche le pattern : loin → proche → loin dans les 10 derniers samples */
    int min_d = 9999, min_pos = -1;
    for (int i = 0; i < 10 && i < s_count; i++) {
        int d = hist(i);
        if (d > 0 && d < min_d) {
            min_d = d;
            min_pos = i;
        }
    }
    if (min_pos < 1 || min_pos > 8) return false;

    /* Verifier qu'avant le min c'etait plus loin */
    int before = -1;
    for (int i = min_pos + 1; i < min_pos + 4 && i < s_count; i++) {
        int d = hist(i);
        if (d > 0) { before = d; break; }
    }

    /* Verifier qu'apres le min c'est plus loin */
    int after = -1;
    for (int i = min_pos - 1; i >= 0; i--) {
        int d = hist(i);
        if (d > 0) { after = d; break; }
    }

    if (before < 0 || after < 0) return false;
    return (before - min_d >= PUSH_DROP_CM && after - min_d >= PUSH_RISE_CM);
}

/* Detecte le geste SWIPE : passage rapide devant le capteur */
static bool detect_swipe(void)
{
    if (s_count < 6) return false;

    /* Cherche : loin → proche → loin en < SWIPE_MAX_TICKS */
    int close_start = -1;
    for (int i = 0; i < 8 && i < s_count; i++) {
        int d = hist(i);
        if (d > 0 && d < SWIPE_CLOSE_CM) {
            if (close_start < 0) close_start = i;
        } else if (close_start >= 0) {
            /* On est sorti de la zone proche */
            int duration = i - close_start;
            if (duration > 0 && duration <= SWIPE_MAX_TICKS) {
                /* Verifier qu'avant c'etait loin */
                int before = hist(i);
                if (before > SWIPE_CLOSE_CM + 20) return true;
            }
            close_start = -1;
        }
    }
    return false;
}

void gesture_detect_update(int distance)
{
    s_history[s_idx] = distance;
    s_idx = (s_idx + 1) % HIST_SIZE;
    s_count++;

    if (s_cooldown > 0) {
        s_cooldown--;
        /* Continuer a tracker le hold meme en cooldown */
        if (distance > 0 && distance < HOLD_DIST_CM) {
            s_hold_count++;
        } else {
            s_hold_count = 0;
        }
        return;
    }

    /* Detection HOLD (en continu, pas de cooldown necessaire) */
    if (distance > 0 && distance < HOLD_DIST_CM) {
        s_hold_count++;
        /* Verifier la stabilite */
        if (s_hold_count >= HOLD_MIN_TICKS) {
            int min_d = 9999, max_d = 0;
            for (int i = 0; i < 5; i++) {
                int d = hist(i);
                if (d > 0) {
                    if (d < min_d) min_d = d;
                    if (d > max_d) max_d = d;
                }
            }
            if ((max_d - min_d) < HOLD_VARIANCE_CM) {
                if (s_gesture != GESTURE_HOLD) {
                    s_gesture = GESTURE_HOLD;
                    ESP_LOGI(TAG, "Geste: HOLD (dist=%dcm)", distance);
                    /* Pas de cooldown pour hold — il persiste */
                }
            }
        }
    } else {
        if (s_hold_count > 0 && s_gesture == GESTURE_HOLD) {
            s_gesture = GESTURE_NONE; /* fin du hold */
        }
        s_hold_count = 0;
    }

    /* Detection des gestes ponctuels */
    if (s_gesture == GESTURE_HOLD) return; /* hold prend priorite */

    if (detect_wave()) {
        s_gesture = GESTURE_WAVE;
        s_cooldown = GESTURE_COOLDOWN;
        ESP_LOGI(TAG, "Geste: WAVE");
    } else if (detect_push()) {
        s_gesture = GESTURE_PUSH;
        s_cooldown = GESTURE_COOLDOWN;
        ESP_LOGI(TAG, "Geste: PUSH");
    } else if (detect_swipe()) {
        s_gesture = GESTURE_SWIPE;
        s_cooldown = GESTURE_COOLDOWN;
        ESP_LOGI(TAG, "Geste: SWIPE");
    }
}

gesture_t gesture_detect_poll(void)
{
    gesture_t g = s_gesture;
    if (g != GESTURE_HOLD) {
        s_gesture = GESTURE_NONE; /* consomme le geste (sauf hold) */
    }
    return g;
}

gesture_t gesture_detect_peek(void)
{
    return s_gesture;
}

const char *gesture_detect_name(gesture_t g)
{
    switch (g) {
    case GESTURE_WAVE:  return "wave";
    case GESTURE_PUSH:  return "push";
    case GESTURE_HOLD:  return "hold";
    case GESTURE_SWIPE: return "swipe";
    default:            return "none";
    }
}

bool gesture_detect_is_hand_close(void)
{
    return s_hold_count >= 3; /* proche depuis au moins 600ms */
}

int gesture_detect_avg_distance(void)
{
    int sum = 0, n = 0;
    for (int i = 0; i < 5; i++) {
        int d = hist(i);
        if (d > 0) { sum += d; n++; }
    }
    return n > 0 ? sum / n : -1;
}
