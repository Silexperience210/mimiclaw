#include "body_animator.h"
#include "hardware/servo_driver.h"
#include "hardware/ultrasonic.h"
#include "mimi_config.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_random.h"

#include <math.h>
#include <string.h>

static const char *TAG = "body_anim";

static display_state_t s_state = DISPLAY_IDLE;
static lobster_mood_t  s_mood  = MOOD_NEUTRAL;
static int s_last_distance     = -1;
static bool s_presence_active  = false;
static uint32_t s_tick         = 0;

/* Tracking : angle cible de la tete pendant le scan */
static int s_scan_target_h     = 90;  /* angle H ou l'objet est le plus proche */
static int s_scan_dir          = 1;   /* direction du balayage (+1/-1) */
static int s_scan_best_dist    = 999; /* plus courte distance trouvee pendant un sweep */
static int s_scan_best_angle   = 90;  /* angle correspondant */
static bool s_scan_locked      = false; /* true = objet localise, on suit */
static int s_lock_lost_count   = 0;    /* compteur perte de lock */

/* Historique distance pour detecter mouvement */
static int s_dist_history[5]   = {-1, -1, -1, -1, -1};
static int s_dist_idx          = 0;

/* --- Helpers --- */

static uint8_t clamp_angle(int a)
{
    if (a < 0)   return 0;
    if (a > 180) return 180;
    return (uint8_t)a;
}

static uint8_t breathe(uint8_t center, int amplitude, int speed)
{
    float v = sinf((float)s_tick / speed) * amplitude;
    return clamp_angle(center + (int)v);
}

static uint8_t rand_around(uint8_t center, int range)
{
    int r = (esp_random() % (2 * range + 1)) - range;
    return clamp_angle(center + r);
}

/* Interpolation lineaire entre deux valeurs selon un ratio 0.0-1.0 */
static int lerp(int a, int b, float t)
{
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return a + (int)((b - a) * t);
}

/* Detecte si l'objet bouge (variation dans l'historique) */
static bool is_object_moving(void)
{
    int valid = 0, min_d = 999, max_d = 0;
    for (int i = 0; i < 5; i++) {
        if (s_dist_history[i] > 0) {
            if (s_dist_history[i] < min_d) min_d = s_dist_history[i];
            if (s_dist_history[i] > max_d) max_d = s_dist_history[i];
            valid++;
        }
    }
    if (valid < 3) return false;
    return (max_d - min_d) > 15; /* >15cm de variation = mouvement */
}

/* ================================================================
 * TRACKING : balayage + lock
 *
 * Avec 1 seul capteur fixe on ne peut pas connaitre la direction.
 * Mais on peut simuler du tracking :
 * - Balayage horizontal lent (la tete "cherche")
 * - Quand la distance diminue → on note l'angle = direction probable
 * - On "lock" sur cet angle et on reduit les mouvements
 * - Si on perd le signal → on relance le balayage
 * ================================================================ */

static void tracking_update(int dist)
{
    /* Ajouter a l'historique */
    s_dist_history[s_dist_idx] = dist;
    s_dist_idx = (s_dist_idx + 1) % 5;

    if (dist <= 0 || dist > MIMI_US_DETECT_CM) {
        /* Rien detecte — perdre le lock progressivement */
        s_lock_lost_count++;
        if (s_lock_lost_count > 15) { /* ~3s sans signal */
            s_scan_locked = false;
            s_scan_best_dist = 999;
        }
        return;
    }

    s_lock_lost_count = 0;

    if (!s_scan_locked) {
        /* Mode balayage : on cherche */
        int current_h = servo_get_angle(SERVO_HEAD_H);

        if (dist < s_scan_best_dist) {
            s_scan_best_dist = dist;
            s_scan_best_angle = current_h;
        }

        /* Fin d'un sweep complet (bord atteint) → lock sur le meilleur angle */
        if (current_h <= 10 || current_h >= 170) {
            if (s_scan_best_dist < MIMI_US_DETECT_CM) {
                s_scan_target_h = s_scan_best_angle;
                s_scan_locked = true;
                ESP_LOGI(TAG, "LOCK on angle %d (dist=%dcm)", s_scan_target_h, s_scan_best_dist);
            }
            /* Reset pour prochain sweep */
            s_scan_best_dist = 999;
            s_scan_dir = -s_scan_dir;
        }
    } else {
        /* Mode lock : on reste sur l'angle mais on ajuste finement */
        /* Si la distance augmente beaucoup → l'objet a peut-etre bouge */
        if (dist > s_scan_best_dist + 50) {
            s_scan_locked = false;
            s_scan_best_dist = 999;
            ESP_LOGI(TAG, "LOCK lost — rescanning");
        } else {
            s_scan_best_dist = dist; /* mise a jour */
        }
    }
}

/* ================================================================
 * ANIMATIONS PAR DISTANCE
 *
 * LOIN  (150-300cm) : "C'est quoi ca?!" — grands mouvements de recherche
 *                      Tete balaye 0°-180°, pinces s'agitent
 *
 * MOYEN (50-150cm)  : "Ca approche!" — mouvements moyens, semi-focus
 *                      Tete oscille ±40° autour du lock, pinces ouvertes
 *
 * PROCHE (< 50cm)   : "T'es la!" — focus laser, micro-mouvements
 *                      Tete fixe sur le lock ±5°, pinces en position
 * ================================================================ */

/* LOIN : grands balayages de recherche */
static void anim_presence_far(int dist)
{
    (void)dist;

    if (!s_scan_locked) {
        /* Balayage complet 0-180 — cherche l'objet */
        int current_h = servo_get_angle(SERVO_HEAD_H);
        int next_h = current_h + s_scan_dir * 4; /* pas de 4° = balayage rapide */
        if (next_h > 175) { next_h = 175; s_scan_dir = -1; }
        if (next_h < 5)   { next_h = 5;   s_scan_dir = 1; }
        servo_set_angle_immediate(SERVO_HEAD_H, clamp_angle(next_h));
    } else {
        /* Lock mais loin : grands oscillations autour du target */
        uint8_t hh = breathe(clamp_angle(s_scan_target_h), 60, 8);
        servo_set_angle_immediate(SERVO_HEAD_H, hh);
    }

    /* Tete verticale : cherche haut/bas */
    uint8_t hv = breathe(90, 40, 10);
    servo_set_angle_immediate(SERVO_HEAD_V, hv);

    /* Pinces : grandes ouvertures excitees */
    uint8_t cl = breathe(90, 80, 6);
    servo_set_angle_immediate(SERVO_CLAW_L, cl);
    uint8_t cr = breathe(90, 80, 7); /* legerement desync */
    servo_set_angle_immediate(SERVO_CLAW_R, cr);
}

/* MOYEN : approche, mouvements moyens, commence a cibler */
static void anim_presence_mid(int dist)
{
    /* Ratio : 1.0 a 150cm → 0.0 a 50cm */
    float ratio = (float)(dist - MIMI_US_NEAR_CM) / (MIMI_US_MID_CM - MIMI_US_NEAR_CM);
    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;

    /* Amplitude tete diminue avec la distance */
    int h_amp = lerp(10, 40, ratio);
    int v_amp = lerp(5, 25, ratio);
    int speed = lerp(20, 10, ratio); /* plus lent quand proche */

    uint8_t center_h = s_scan_locked ? clamp_angle(s_scan_target_h) : 90;
    uint8_t hh = breathe(center_h, h_amp, speed);
    uint8_t hv = breathe(80, v_amp, speed + 5);
    servo_set_angle_immediate(SERVO_HEAD_H, hh);
    servo_set_angle_immediate(SERVO_HEAD_V, hv);

    /* Pinces : ouverture proportionnelle */
    int claw_center = lerp(50, 130, ratio);
    int claw_amp = lerp(10, 40, ratio);
    uint8_t cl = breathe(clamp_angle(claw_center), claw_amp, speed + 2);
    servo_set_angle_immediate(SERVO_CLAW_L, cl);
    servo_set_angle_immediate(SERVO_CLAW_R, cl);
}

/* PROCHE : focus total, micro-mouvements, "stare" */
static void anim_presence_near(int dist)
{
    /* Ratio : 1.0 a 50cm → 0.0 a 0cm */
    float ratio = (float)dist / MIMI_US_NEAR_CM;
    if (ratio > 1.0f) ratio = 1.0f;

    uint8_t center_h = s_scan_locked ? clamp_angle(s_scan_target_h) : 90;

    /* Tete quasi fixe — micro-tremblements comme si elle fixe */
    int h_amp = lerp(2, 8, ratio);
    uint8_t hh = breathe(center_h, h_amp, 25);
    servo_set_angle_immediate(SERVO_HEAD_H, hh);

    /* Tete legèrement relevee — "regarde vers le haut" */
    uint8_t hv = breathe(clamp_angle(lerp(100, 85, ratio)), 3, 30);
    servo_set_angle_immediate(SERVO_HEAD_V, hv);

    /* Pinces : petits mouvements — "pret a attraper" */
    bool moving = is_object_moving();
    if (moving) {
        /* Objet bouge tout pres → pinces excitees */
        uint8_t cl = breathe(120, 40, 4);
        servo_set_angle_immediate(SERVO_CLAW_L, cl);
        servo_set_angle_immediate(SERVO_CLAW_R, cl);
    } else {
        /* Objet statique tout pres → pinces semi-ouvertes, stables */
        uint8_t cl = breathe(70, 5, 40);
        servo_set_angle_immediate(SERVO_CLAW_L, cl);
        servo_set_angle_immediate(SERVO_CLAW_R, cl);
    }
}

/* Dispatch presence selon distance */
static void anim_presence(int dist)
{
    if (dist > MIMI_US_MID_CM) {
        anim_presence_far(dist);
    } else if (dist > MIMI_US_NEAR_CM) {
        anim_presence_mid(dist);
    } else {
        anim_presence_near(dist);
    }
}

/* --- Animations par etat (inchangees) --- */

/* IDLE : respiration calme */
static void anim_idle(void)
{
    uint8_t hh = breathe(90, 12, 30);
    uint8_t hv = breathe(90, 8, 40);
    servo_set_angle_immediate(SERVO_HEAD_H, hh);
    servo_set_angle_immediate(SERVO_HEAD_V, hv);

    uint8_t cl = breathe(20, 15, 50);
    servo_set_angle_immediate(SERVO_CLAW_L, cl);
    servo_set_angle_immediate(SERVO_CLAW_R, cl);
}

/* THINKING : tete penchee, pince gratte le menton */
static void anim_thinking(void)
{
    servo_set_angle_immediate(SERVO_HEAD_H, 55);
    uint8_t hv = breathe(75, 10, 20);
    servo_set_angle_immediate(SERVO_HEAD_V, hv);

    uint8_t cl = breathe(70, 30, 8);
    servo_set_angle_immediate(SERVO_CLAW_L, cl);
    servo_set_angle_immediate(SERVO_CLAW_R, 10);
}

/* MESSAGE recu : surprise */
static void anim_message_received(void)
{
    servo_set_angle_immediate(SERVO_HEAD_H, 90);
    servo_set_angle_immediate(SERVO_HEAD_V, 120);
    servo_set_angle_immediate(SERVO_CLAW_L, 170);
    servo_set_angle_immediate(SERVO_CLAW_R, 170);
}

/* PROUD : fier */
static void anim_proud(void)
{
    servo_set_angle_immediate(SERVO_HEAD_H, 90);
    uint8_t hv = breathe(90, 20, 6);
    servo_set_angle_immediate(SERVO_HEAD_V, hv);
    servo_set_angle_immediate(SERVO_CLAW_L, 175);
    servo_set_angle_immediate(SERVO_CLAW_R, 175);
}

/* SLEEPY : tete tombe */
static void anim_sleepy(void)
{
    uint8_t hv = breathe(45, 5, 60);
    servo_set_angle_immediate(SERVO_HEAD_H, 90);
    servo_set_angle_immediate(SERVO_HEAD_V, hv);
    servo_set_angle_immediate(SERVO_CLAW_L, 5);
    servo_set_angle_immediate(SERVO_CLAW_R, 5);
}

/* SCREENSAVER : mouvements lents aleatoires */
static void anim_screensaver(void)
{
    if ((s_tick % 60) == 0) {
        servo_set_angle(SERVO_HEAD_H, rand_around(90, 80));
        servo_set_angle(SERVO_HEAD_V, rand_around(90, 60));
        servo_set_angle(SERVO_CLAW_L, rand_around(90, 80));
        servo_set_angle(SERVO_CLAW_R, rand_around(90, 80));
    }
}

/* Au revoir : grand signe de pince */
static void anim_goodbye(void)
{
    for (int i = 0; i < 5; i++) {
        servo_set_angle_immediate(SERVO_CLAW_R, 175);
        servo_set_angle_immediate(SERVO_HEAD_H, 120);
        vTaskDelay(pdMS_TO_TICKS(180));
        servo_set_angle_immediate(SERVO_CLAW_R, 30);
        servo_set_angle_immediate(SERVO_HEAD_H, 60);
        vTaskDelay(pdMS_TO_TICKS(180));
    }
    servo_set_angle_immediate(SERVO_HEAD_H, 90);
}

/* --- Animations predefinies (appelees par tool_servo) --- */

static void run_wave(void)
{
    for (int i = 0; i < 5; i++) {
        servo_set_angle_immediate(SERVO_CLAW_R, 175);
        servo_set_angle_immediate(SERVO_CLAW_L, 10);
        vTaskDelay(pdMS_TO_TICKS(160));
        servo_set_angle_immediate(SERVO_CLAW_R, 30);
        servo_set_angle_immediate(SERVO_CLAW_L, 150);
        vTaskDelay(pdMS_TO_TICKS(160));
    }
}

static void run_nod_yes(void)
{
    for (int i = 0; i < 4; i++) {
        servo_set_angle_immediate(SERVO_HEAD_V, 130);
        vTaskDelay(pdMS_TO_TICKS(130));
        servo_set_angle_immediate(SERVO_HEAD_V, 50);
        vTaskDelay(pdMS_TO_TICKS(130));
    }
    servo_set_angle_immediate(SERVO_HEAD_V, 90);
}

static void run_nod_no(void)
{
    for (int i = 0; i < 4; i++) {
        servo_set_angle_immediate(SERVO_HEAD_H, 145);
        vTaskDelay(pdMS_TO_TICKS(130));
        servo_set_angle_immediate(SERVO_HEAD_H, 35);
        vTaskDelay(pdMS_TO_TICKS(130));
    }
    servo_set_angle_immediate(SERVO_HEAD_H, 90);
}

static void run_celebrate(void)
{
    for (int i = 0; i < 5; i++) {
        servo_set_angle_immediate(SERVO_CLAW_L, 180);
        servo_set_angle_immediate(SERVO_CLAW_R, 180);
        servo_set_angle_immediate(SERVO_HEAD_V, 130);
        servo_set_angle_immediate(SERVO_HEAD_H, 50 + (i * 20));
        vTaskDelay(pdMS_TO_TICKS(160));
        servo_set_angle_immediate(SERVO_CLAW_L, 10);
        servo_set_angle_immediate(SERVO_CLAW_R, 10);
        servo_set_angle_immediate(SERVO_HEAD_V, 50);
        vTaskDelay(pdMS_TO_TICKS(160));
    }
    servo_set_angle_immediate(SERVO_HEAD_H, 90);
    servo_set_angle_immediate(SERVO_HEAD_V, 90);
}

/* Joue une animation nommee (appele depuis tool_servo) */
void body_animator_play(const char *name)
{
    if (strcmp(name, "wave") == 0)           run_wave();
    else if (strcmp(name, "nod_yes") == 0)   run_nod_yes();
    else if (strcmp(name, "nod_no") == 0)    run_nod_no();
    else if (strcmp(name, "celebrate") == 0)  run_celebrate();
    else if (strcmp(name, "think") == 0) {
        for (int i = 0; i < 15; i++) {
            s_tick++;
            anim_thinking();
            vTaskDelay(pdMS_TO_TICKS(200));
        }
    } else if (strcmp(name, "sleep") == 0) {
        for (int i = 0; i < 15; i++) {
            s_tick++;
            anim_sleepy();
            vTaskDelay(pdMS_TO_TICKS(200));
        }
    } else {
        ESP_LOGW(TAG, "Animation inconnue: %s", name);
    }
}

/* --- Task principale --- */

static void body_animator_task(void *arg)
{
    ESP_LOGI(TAG, "Body animator started (detect=%dcm, mid=%dcm, near=%dcm)",
             MIMI_US_DETECT_CM, MIMI_US_MID_CM, MIMI_US_NEAR_CM);

    while (1) {
        s_tick++;

        /* Lecture ultrason */
        int dist = ultrasonic_read_cm();
        bool was_present = s_presence_active;

        if (dist > 0 && dist < MIMI_US_DETECT_CM) {
            s_presence_active = true;
        } else if (dist > MIMI_US_DETECT_CM + 20 || dist < 0) {
            s_presence_active = false;
        }
        s_last_distance = dist;

        /* Mise a jour du tracking */
        tracking_update(dist);

        /* Detection depart */
        if (was_present && !s_presence_active && s_state == DISPLAY_IDLE) {
            anim_goodbye();
            s_scan_locked = false;
            s_scan_best_dist = 999;
        }

        /* Choix animation selon etat + presence + distance */
        switch (s_state) {
        case DISPLAY_IDLE:
            if (s_mood == MOOD_SLEEPY) {
                anim_sleepy();
            } else if (s_mood == MOOD_PROUD) {
                anim_proud();
            } else if (s_mood == MOOD_EXCITED) {
                anim_message_received();
            } else if (s_presence_active && dist > 0) {
                anim_presence(dist);
            } else {
                anim_idle();
            }
            break;

        case DISPLAY_THINKING:
            anim_thinking();
            break;

        case DISPLAY_MESSAGE:
            anim_proud();
            break;

        case DISPLAY_SCREENSAVER:
            anim_screensaver();
            break;

        case DISPLAY_SLEEP:
        case DISPLAY_PORTAL:
        default:
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(MIMI_US_POLL_MS));
    }
}

esp_err_t body_animator_init(void)
{
    BaseType_t ret = xTaskCreatePinnedToCore(
        body_animator_task, "body_anim",
        MIMI_BODY_STACK, NULL,
        MIMI_BODY_PRIO, NULL, MIMI_BODY_CORE);

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create body animator task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Body animator init OK");
    return ESP_OK;
}

void body_animator_set_state(display_state_t state)
{
    s_state = state;
}

void body_animator_set_mood(lobster_mood_t mood)
{
    s_mood = mood;
}

void body_animator_sleep(void)
{
    ESP_LOGI(TAG, "Servos en position repos");
    servo_set_angle_immediate(SERVO_HEAD_H, 90);
    servo_set_angle_immediate(SERVO_HEAD_V, 90);
    servo_set_angle_immediate(SERVO_CLAW_L, 0);
    servo_set_angle_immediate(SERVO_CLAW_R, 0);
}

int body_animator_get_distance(void)
{
    return s_last_distance;
}
