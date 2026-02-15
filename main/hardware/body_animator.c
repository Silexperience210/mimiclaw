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
static int s_last_distance     = -1;   /* derniere distance capteur */
static bool s_presence_active  = false; /* quelqu'un devant */
static uint32_t s_tick         = 0;    /* compteur de cycles */

/* Etat precedent pour detecter les transitions */
static display_state_t s_prev_state = DISPLAY_IDLE;
static lobster_mood_t  s_prev_mood  = MOOD_NEUTRAL;
static bool s_prev_presence         = false;

/* --- Helpers --- */

static uint8_t clamp_angle(int a)
{
    if (a < 0)   return 0;
    if (a > 180) return 180;
    return (uint8_t)a;
}

/* Petit mouvement sinusoidal autour d'un centre */
static uint8_t breathe(uint8_t center, int amplitude, int speed)
{
    float v = sinf((float)s_tick / speed) * amplitude;
    return clamp_angle(center + (int)v);
}

/* Angle aleatoire dans [center-range, center+range] */
static uint8_t rand_around(uint8_t center, int range)
{
    int r = (esp_random() % (2 * range + 1)) - range;
    return clamp_angle(center + r);
}

/* --- Animations par etat --- */

/* IDLE : micro-mouvements respiratoires */
static void anim_idle(void)
{
    uint8_t hh = breathe(90, 3, 40);
    uint8_t hv = breathe(90, 2, 50);
    servo_set_angle_immediate(SERVO_HEAD_H, hh);
    servo_set_angle_immediate(SERVO_HEAD_V, hv);

    /* Pinces legere respiration */
    uint8_t cl = breathe(10, 3, 60);
    servo_set_angle_immediate(SERVO_CLAW_L, cl);
    servo_set_angle_immediate(SERVO_CLAW_R, cl);
}

/* IDLE + presence detectee : tete se tourne vers la personne */
static void anim_idle_presence(void)
{
    /* Tete suit la direction (simplifie : on tourne vers le centre-avant) */
    uint8_t hh = breathe(90, 5, 30);
    uint8_t hv = breathe(80, 3, 40);
    servo_set_angle_immediate(SERVO_HEAD_H, hh);
    servo_set_angle_immediate(SERVO_HEAD_V, hv);

    /* Pinces legèrement ouvertes — curiosite */
    servo_set_angle_immediate(SERVO_CLAW_L, 30);
    servo_set_angle_immediate(SERVO_CLAW_R, 30);
}

/* Presence tres proche : excite */
static void anim_near_presence(void)
{
    uint8_t hh = breathe(90, 15, 10);  /* tete tourne vite */
    uint8_t hv = breathe(85, 8, 15);
    servo_set_angle_immediate(SERVO_HEAD_H, hh);
    servo_set_angle_immediate(SERVO_HEAD_V, hv);

    /* Pinces ouvertes */
    uint8_t cl = breathe(120, 20, 8);
    servo_set_angle_immediate(SERVO_CLAW_L, cl);
    servo_set_angle_immediate(SERVO_CLAW_R, cl);
}

/* THINKING : tete penchee, pince G gratte le menton */
static void anim_thinking(void)
{
    servo_set_angle_immediate(SERVO_HEAD_H, 70);  /* tete penchee a gauche */
    uint8_t hv = breathe(80, 3, 30);
    servo_set_angle_immediate(SERVO_HEAD_V, hv);

    /* Pince gauche "gratte menton" */
    uint8_t cl = breathe(60, 15, 12);
    servo_set_angle_immediate(SERVO_CLAW_L, cl);
    servo_set_angle_immediate(SERVO_CLAW_R, 5);  /* pince droite reposee */
}

/* MESSAGE recu : surprise */
static void anim_message_received(void)
{
    /* Transition rapide : pinces s'ouvrent, tete se redresse */
    servo_set_angle_immediate(SERVO_HEAD_H, 90);
    servo_set_angle_immediate(SERVO_HEAD_V, 95);  /* tete legerement en arriere */
    servo_set_angle_immediate(SERVO_CLAW_L, 150);
    servo_set_angle_immediate(SERVO_CLAW_R, 150);
}

/* PROUD : fier — pinces levees, hochement */
static void anim_proud(void)
{
    servo_set_angle_immediate(SERVO_HEAD_H, 90);
    uint8_t hv = breathe(90, 8, 8); /* hochement rapide */
    servo_set_angle_immediate(SERVO_HEAD_V, hv);
    servo_set_angle_immediate(SERVO_CLAW_L, 160);
    servo_set_angle_immediate(SERVO_CLAW_R, 160);
}

/* SLEEPY : tete tombe, pinces relachees */
static void anim_sleepy(void)
{
    uint8_t hv = breathe(60, 2, 80); /* respiration lente, tete basse */
    servo_set_angle_immediate(SERVO_HEAD_H, 90);
    servo_set_angle_immediate(SERVO_HEAD_V, hv);
    servo_set_angle_immediate(SERVO_CLAW_L, 5);
    servo_set_angle_immediate(SERVO_CLAW_R, 5);
}

/* SCREENSAVER : mouvements lents aleatoires */
static void anim_screensaver(void)
{
    if ((s_tick % 60) == 0) { /* change position toutes les ~12s */
        servo_set_angle(SERVO_HEAD_H, rand_around(90, 40));
        servo_set_angle(SERVO_HEAD_V, rand_around(90, 30));
        servo_set_angle(SERVO_CLAW_L, rand_around(45, 45));
        servo_set_angle(SERVO_CLAW_R, rand_around(45, 45));
    }
}

/* Au revoir : pince fait signe */
static void anim_goodbye(void)
{
    for (int i = 0; i < 3; i++) {
        servo_set_angle_immediate(SERVO_CLAW_R, 150);
        vTaskDelay(pdMS_TO_TICKS(200));
        servo_set_angle_immediate(SERVO_CLAW_R, 90);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

/* --- Animations predefinies (appelees par tool_servo) --- */

static void run_wave(void)
{
    for (int i = 0; i < 4; i++) {
        servo_set_angle_immediate(SERVO_CLAW_R, 160);
        vTaskDelay(pdMS_TO_TICKS(180));
        servo_set_angle_immediate(SERVO_CLAW_R, 80);
        vTaskDelay(pdMS_TO_TICKS(180));
    }
    servo_set_angle_immediate(SERVO_CLAW_R, servo_get_angle(SERVO_CLAW_R));
}

static void run_nod_yes(void)
{
    for (int i = 0; i < 3; i++) {
        servo_set_angle_immediate(SERVO_HEAD_V, 110);
        vTaskDelay(pdMS_TO_TICKS(150));
        servo_set_angle_immediate(SERVO_HEAD_V, 70);
        vTaskDelay(pdMS_TO_TICKS(150));
    }
    servo_set_angle_immediate(SERVO_HEAD_V, 90);
}

static void run_nod_no(void)
{
    for (int i = 0; i < 3; i++) {
        servo_set_angle_immediate(SERVO_HEAD_H, 120);
        vTaskDelay(pdMS_TO_TICKS(150));
        servo_set_angle_immediate(SERVO_HEAD_H, 60);
        vTaskDelay(pdMS_TO_TICKS(150));
    }
    servo_set_angle_immediate(SERVO_HEAD_H, 90);
}

static void run_celebrate(void)
{
    for (int i = 0; i < 4; i++) {
        servo_set_angle_immediate(SERVO_CLAW_L, 170);
        servo_set_angle_immediate(SERVO_CLAW_R, 170);
        servo_set_angle_immediate(SERVO_HEAD_V, 110);
        vTaskDelay(pdMS_TO_TICKS(200));
        servo_set_angle_immediate(SERVO_CLAW_L, 40);
        servo_set_angle_immediate(SERVO_CLAW_R, 40);
        servo_set_angle_immediate(SERVO_HEAD_V, 80);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    servo_set_angle_immediate(SERVO_HEAD_V, 90);
}

/* Joue une animation nommee (appele depuis tool_servo) */
void body_animator_play(const char *name)
{
    if (strcmp(name, "wave") == 0)      run_wave();
    else if (strcmp(name, "nod_yes") == 0)  run_nod_yes();
    else if (strcmp(name, "nod_no") == 0)   run_nod_no();
    else if (strcmp(name, "celebrate") == 0) run_celebrate();
    else if (strcmp(name, "think") == 0) {
        /* Joue l'anim thinking pendant 3s */
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
    ESP_LOGI(TAG, "Body animator started");

    while (1) {
        s_tick++;

        /* Lecture ultrason */
        int dist = ultrasonic_read_cm();
        bool was_present = s_presence_active;

        if (dist > 0 && dist < MIMI_US_DETECT_CM) {
            s_presence_active = true;
        } else if (dist > MIMI_US_DETECT_CM + 10 || dist < 0) {
            /* Hysteresis pour eviter le clignotement */
            s_presence_active = false;
        }
        s_last_distance = dist;

        /* Detection transition depart */
        if (was_present && !s_presence_active &&
            s_state == DISPLAY_IDLE) {
            anim_goodbye();
        }

        /* Choix animation selon etat + presence */
        switch (s_state) {
        case DISPLAY_IDLE:
            if (s_mood == MOOD_SLEEPY) {
                anim_sleepy();
            } else if (s_mood == MOOD_PROUD) {
                anim_proud();
            } else if (s_mood == MOOD_EXCITED) {
                anim_message_received();
            } else if (s_presence_active && dist > 0 && dist < MIMI_US_NEAR_CM) {
                anim_near_presence();
            } else if (s_presence_active) {
                anim_idle_presence();
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
            /* Pas d'animation */
            break;
        }

        s_prev_state = s_state;
        s_prev_mood  = s_mood;
        s_prev_presence = s_presence_active;

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

/* Accesseur pour tool_servo */
int body_animator_get_distance(void)
{
    return s_last_distance;
}
