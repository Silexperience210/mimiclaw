#include "display_ui.h"
#include "display_hal.h"
#include "lobster_sprite.h"
#include "mimi_config.h"

#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_random.h"

static const char *TAG = "disp_ui";

/* ---- Font bitmap 5x7 compacte (ASCII 32-126) ---- */
/* Chaque caractere = 5 octets, chaque octet = une colonne de 7 bits */
static const uint8_t font5x7[] = {
    0x00,0x00,0x00,0x00,0x00, /* espace */
    0x00,0x00,0x5F,0x00,0x00, /* ! */
    0x00,0x07,0x00,0x07,0x00, /* " */
    0x14,0x7F,0x14,0x7F,0x14, /* # */
    0x24,0x2A,0x7F,0x2A,0x12, /* $ */
    0x23,0x13,0x08,0x64,0x62, /* % */
    0x36,0x49,0x55,0x22,0x50, /* & */
    0x00,0x05,0x03,0x00,0x00, /* ' */
    0x00,0x1C,0x22,0x41,0x00, /* ( */
    0x00,0x41,0x22,0x1C,0x00, /* ) */
    0x08,0x2A,0x1C,0x2A,0x08, /* * */
    0x08,0x08,0x3E,0x08,0x08, /* + */
    0x00,0x50,0x30,0x00,0x00, /* , */
    0x08,0x08,0x08,0x08,0x08, /* - */
    0x00,0x60,0x60,0x00,0x00, /* . */
    0x20,0x10,0x08,0x04,0x02, /* / */
    0x3E,0x51,0x49,0x45,0x3E, /* 0 */
    0x00,0x42,0x7F,0x40,0x00, /* 1 */
    0x42,0x61,0x51,0x49,0x46, /* 2 */
    0x21,0x41,0x45,0x4B,0x31, /* 3 */
    0x18,0x14,0x12,0x7F,0x10, /* 4 */
    0x27,0x45,0x45,0x45,0x39, /* 5 */
    0x3C,0x4A,0x49,0x49,0x30, /* 6 */
    0x01,0x71,0x09,0x05,0x03, /* 7 */
    0x36,0x49,0x49,0x49,0x36, /* 8 */
    0x06,0x49,0x49,0x29,0x1E, /* 9 */
    0x00,0x36,0x36,0x00,0x00, /* : */
    0x00,0x56,0x36,0x00,0x00, /* ; */
    0x00,0x08,0x14,0x22,0x41, /* < */
    0x14,0x14,0x14,0x14,0x14, /* = */
    0x41,0x22,0x14,0x08,0x00, /* > */
    0x02,0x01,0x51,0x09,0x06, /* ? */
    0x3E,0x41,0x5D,0x55,0x5E, /* @ */
    0x7E,0x09,0x09,0x09,0x7E, /* A */
    0x7F,0x49,0x49,0x49,0x36, /* B */
    0x3E,0x41,0x41,0x41,0x22, /* C */
    0x7F,0x41,0x41,0x22,0x1C, /* D */
    0x7F,0x49,0x49,0x49,0x41, /* E */
    0x7F,0x09,0x09,0x01,0x01, /* F */
    0x3E,0x41,0x41,0x51,0x32, /* G */
    0x7F,0x08,0x08,0x08,0x7F, /* H */
    0x00,0x41,0x7F,0x41,0x00, /* I */
    0x20,0x40,0x41,0x3F,0x01, /* J */
    0x7F,0x08,0x14,0x22,0x41, /* K */
    0x7F,0x40,0x40,0x40,0x40, /* L */
    0x7F,0x02,0x04,0x02,0x7F, /* M */
    0x7F,0x04,0x08,0x10,0x7F, /* N */
    0x3E,0x41,0x41,0x41,0x3E, /* O */
    0x7F,0x09,0x09,0x09,0x06, /* P */
    0x3E,0x41,0x51,0x21,0x5E, /* Q */
    0x7F,0x09,0x19,0x29,0x46, /* R */
    0x46,0x49,0x49,0x49,0x31, /* S */
    0x01,0x01,0x7F,0x01,0x01, /* T */
    0x3F,0x40,0x40,0x40,0x3F, /* U */
    0x1F,0x20,0x40,0x20,0x1F, /* V */
    0x7F,0x20,0x18,0x20,0x7F, /* W */
    0x63,0x14,0x08,0x14,0x63, /* X */
    0x03,0x04,0x78,0x04,0x03, /* Y */
    0x61,0x51,0x49,0x45,0x43, /* Z */
    0x00,0x00,0x7F,0x41,0x41, /* [ */
    0x02,0x04,0x08,0x10,0x20, /* \ */
    0x41,0x41,0x7F,0x00,0x00, /* ] */
    0x04,0x02,0x01,0x02,0x04, /* ^ */
    0x40,0x40,0x40,0x40,0x40, /* _ */
    0x00,0x01,0x02,0x04,0x00, /* ` */
    0x20,0x54,0x54,0x54,0x78, /* a */
    0x7F,0x48,0x44,0x44,0x38, /* b */
    0x38,0x44,0x44,0x44,0x20, /* c */
    0x38,0x44,0x44,0x48,0x7F, /* d */
    0x38,0x54,0x54,0x54,0x18, /* e */
    0x08,0x7E,0x09,0x01,0x02, /* f */
    0x08,0x54,0x54,0x54,0x3C, /* g */
    0x7F,0x08,0x04,0x04,0x78, /* h */
    0x00,0x44,0x7D,0x40,0x00, /* i */
    0x20,0x40,0x44,0x3D,0x00, /* j */
    0x00,0x7F,0x10,0x28,0x44, /* k */
    0x00,0x41,0x7F,0x40,0x00, /* l */
    0x7C,0x04,0x18,0x04,0x78, /* m */
    0x7C,0x08,0x04,0x04,0x78, /* n */
    0x38,0x44,0x44,0x44,0x38, /* o */
    0x7C,0x14,0x14,0x14,0x08, /* p */
    0x08,0x14,0x14,0x18,0x7C, /* q */
    0x7C,0x08,0x04,0x04,0x08, /* r */
    0x48,0x54,0x54,0x54,0x20, /* s */
    0x04,0x3F,0x44,0x40,0x20, /* t */
    0x3C,0x40,0x40,0x20,0x7C, /* u */
    0x1C,0x20,0x40,0x20,0x1C, /* v */
    0x3C,0x40,0x30,0x40,0x3C, /* w */
    0x44,0x28,0x10,0x28,0x44, /* x */
    0x0C,0x50,0x50,0x50,0x3C, /* y */
    0x44,0x64,0x54,0x4C,0x44, /* z */
    0x00,0x08,0x36,0x41,0x00, /* { */
    0x00,0x00,0x7F,0x00,0x00, /* | */
    0x00,0x41,0x36,0x08,0x00, /* } */
    0x08,0x04,0x08,0x10,0x08, /* ~ */
};

#define FONT_W 5
#define FONT_H 7
#define CHAR_SPACING 1

/* ---- Couleurs ---- */
#define COL_BG       0x0000  /* noir */
#define COL_TEXT     0xFFFF  /* blanc */
#define COL_DIM      0x7BEF  /* gris */
#define COL_ACCENT   0xFCA5  /* orange */
#define COL_THINK    0x06FF  /* cyan */
#define COL_GREEN    0x07E0  /* vert */
#define COL_RED      0xF800  /* rouge */

/* ---- Etat interne ---- */
static display_state_t s_state = DISPLAY_IDLE;
static SemaphoreHandle_t s_mutex = NULL;
static char s_message[256] = {0};
static bool s_wifi_ok = false;
static char s_ip[20] = {0};
static uint32_t s_frame_count = 0;

/* ---- Buffer de dessin par bande ---- */
static uint16_t s_line_buf[MIMI_DISP_WIDTH * MIMI_DISP_BUF_LINES];

/* ---- Dessin bas-niveau ---- */

static void fill_rect(int x, int y, int w, int h, uint16_t color)
{
    /* Remplit directement via le HAL, une bande a la fois */
    int buf_size = w * h;
    if (buf_size > MIMI_DISP_WIDTH * MIMI_DISP_BUF_LINES) {
        /* Trop grand, decouper en bandes */
        int lines_per_batch = MIMI_DISP_BUF_LINES;
        for (int row = 0; row < h; row += lines_per_batch) {
            int batch_h = (row + lines_per_batch > h) ? (h - row) : lines_per_batch;
            int count = w * batch_h;
            for (int i = 0; i < count; i++) s_line_buf[i] = color;
            display_hal_flush(x, y + row, w, batch_h, s_line_buf);
        }
    } else {
        for (int i = 0; i < buf_size; i++) s_line_buf[i] = color;
        display_hal_flush(x, y, w, h, s_line_buf);
    }
}

static void draw_char(int x, int y, char c, uint16_t color, uint16_t bg, int scale)
{
    if (c < 32 || c > 126) c = '?';
    const uint8_t *glyph = &font5x7[(c - 32) * 5];
    int sw = FONT_W * scale;
    int sh = FONT_H * scale;

    if (sw * sh > MIMI_DISP_WIDTH * MIMI_DISP_BUF_LINES) return;

    int idx = 0;
    for (int row = 0; row < FONT_H; row++) {
        for (int sy = 0; sy < scale; sy++) {
            for (int col = 0; col < FONT_W; col++) {
                uint16_t px = (glyph[col] & (1 << row)) ? color : bg;
                for (int sx = 0; sx < scale; sx++) {
                    s_line_buf[idx++] = px;
                }
            }
        }
    }
    display_hal_flush(x, y, sw, sh, s_line_buf);
}

static void draw_string(int x, int y, const char *str, uint16_t color, uint16_t bg, int scale)
{
    int cx = x;
    while (*str) {
        if (*str == '\n') {
            y += (FONT_H + 2) * scale;
            cx = x;
            str++;
            continue;
        }
        draw_char(cx, y, *str, color, bg, scale);
        cx += (FONT_W + CHAR_SPACING) * scale;
        if (cx + FONT_W * scale > MIMI_DISP_WIDTH) {
            /* Retour a la ligne auto */
            y += (FONT_H + 2) * scale;
            cx = x;
        }
        str++;
    }
}

static void draw_sprite(int x, int y, const uint16_t *sprite, int w, int h, int scale)
{
    /* Dessine un sprite avec echelle, par bandes de lignes */
    for (int row = 0; row < h; row++) {
        /* Construire une bande de scale lignes */
        int idx = 0;
        for (int sy = 0; sy < scale; sy++) {
            for (int col = 0; col < w; col++) {
                uint16_t px = sprite[row * w + col];
                for (int sx = 0; sx < scale; sx++) {
                    s_line_buf[idx++] = px;
                }
            }
        }
        display_hal_flush(x, y + row * scale, w * scale, scale, s_line_buf);
    }
}

/* ---- Ecrans ---- */

static void draw_status_bar(void)
{
    /* Barre de status en haut : WiFi icon + IP */
    fill_rect(0, 0, MIMI_DISP_WIDTH, 12, COL_BG);

    /* Indicateur WiFi */
    uint16_t wifi_col = s_wifi_ok ? COL_GREEN : COL_RED;
    draw_char(2, 2, s_wifi_ok ? 'W' : '!', wifi_col, COL_BG, 1);

    /* IP */
    if (s_wifi_ok && s_ip[0]) {
        draw_string(12, 2, s_ip, COL_DIM, COL_BG, 1);
    }
}

static void draw_idle(void)
{
    fill_rect(0, 0, MIMI_DISP_WIDTH, MIMI_DISP_HEIGHT, COL_BG);
    draw_status_bar();

    /* Lobster centre */
    int sprite_scale = 3;
    int sx = (MIMI_DISP_WIDTH - LOBSTER_W * sprite_scale) / 2;
    int sy = 30;

    /* Clignement des yeux : ~200ms toutes les 3-5 secondes */
    bool blink = false;
    if (s_frame_count % (MIMI_DISP_FPS_IDLE * 4) < 1) {
        blink = true;
    }

    const uint16_t *frame = blink ? lobster_blink : lobster_idle;
    draw_sprite(sx, sy, frame, LOBSTER_W, LOBSTER_H, sprite_scale);

    /* Titre sous le lobster */
    draw_string(28, sy + LOBSTER_H * sprite_scale + 10, "LilyClaw", COL_ACCENT, COL_BG, 2);

    /* Sous-titre */
    draw_string(30, sy + LOBSTER_H * sprite_scale + 35, "AI Assistant", COL_DIM, COL_BG, 1);
}

static void draw_thinking(void)
{
    fill_rect(0, 0, MIMI_DISP_WIDTH, MIMI_DISP_HEIGHT, COL_BG);
    draw_status_bar();

    /* Lobster (toujours visible) */
    int sprite_scale = 3;
    int sx = (MIMI_DISP_WIDTH - LOBSTER_W * sprite_scale) / 2;
    int sy = 20;

    /* Alterner entre frames pour animation */
    int anim = (s_frame_count / 4) % 2;
    const uint16_t *frame = anim ? lobster_blink : lobster_idle;
    draw_sprite(sx, sy, frame, LOBSTER_W, LOBSTER_H, sprite_scale);

    /* "Thinking" avec dots animes */
    int dots = (s_frame_count / 5) % 4;
    char think_text[16] = "Thinking";
    for (int i = 0; i < dots; i++) strcat(think_text, ".");

    draw_string(20, sy + LOBSTER_H * sprite_scale + 10, think_text, COL_THINK, COL_BG, 2);

    /* Barre de progression animee */
    int bar_y = sy + LOBSTER_H * sprite_scale + 40;
    int bar_w = 120;
    int bar_x = (MIMI_DISP_WIDTH - bar_w) / 2;
    fill_rect(bar_x, bar_y, bar_w, 3, 0x2104);  /* fond gris fonce */

    int progress = (s_frame_count * 3) % bar_w;
    int seg_w = 30;
    int seg_start = progress;
    if (seg_start + seg_w > bar_w) seg_w = bar_w - seg_start;
    fill_rect(bar_x + seg_start, bar_y, seg_w, 3, COL_THINK);
}

static void draw_message(void)
{
    fill_rect(0, 0, MIMI_DISP_WIDTH, MIMI_DISP_HEIGHT, COL_BG);
    draw_status_bar();

    draw_string(4, 16, "Last message:", COL_ACCENT, COL_BG, 1);

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (s_message[0]) {
        draw_string(4, 30, s_message, COL_TEXT, COL_BG, 1);
    } else {
        draw_string(4, 30, "(aucun message)", COL_DIM, COL_BG, 1);
    }
    xSemaphoreGive(s_mutex);
}

static void draw_portal(void)
{
    fill_rect(0, 0, MIMI_DISP_WIDTH, MIMI_DISP_HEIGHT, COL_BG);

    /* Lobster petit */
    draw_sprite(55, 10, lobster_idle, LOBSTER_W, LOBSTER_H, 2);

    draw_string(20, 80, "Setup Mode", COL_ACCENT, COL_BG, 2);
    draw_string(4, 110, "WiFi:", COL_DIM, COL_BG, 1);
    draw_string(4, 122, MIMI_PORTAL_AP_SSID, COL_TEXT, COL_BG, 1);
    draw_string(4, 140, "Pass:", COL_DIM, COL_BG, 1);
    draw_string(4, 152, MIMI_PORTAL_AP_PASS, COL_TEXT, COL_BG, 1);
    draw_string(4, 175, "Open browser:", COL_DIM, COL_BG, 1);
    draw_string(4, 190, "192.168.4.1", COL_ACCENT, COL_BG, 2);
}

/* ---- Task principale d'affichage ---- */

static void display_task(void *arg)
{
    ESP_LOGI(TAG, "Display task started");

    /* Splash screen */
    fill_rect(0, 0, MIMI_DISP_WIDTH, MIMI_DISP_HEIGHT, COL_BG);
    draw_sprite(37, 40, lobster_idle, LOBSTER_W, LOBSTER_H, 3);
    draw_string(28, 160, "LilyClaw", COL_ACCENT, COL_BG, 2);
    draw_string(40, 190, "Booting...", COL_DIM, COL_BG, 1);
    vTaskDelay(pdMS_TO_TICKS(1500));

    while (1) {
        display_state_t state;
        xSemaphoreTake(s_mutex, portMAX_DELAY);
        state = s_state;
        xSemaphoreGive(s_mutex);

        switch (state) {
        case DISPLAY_IDLE:
            draw_idle();
            break;
        case DISPLAY_THINKING:
            draw_thinking();
            break;
        case DISPLAY_MESSAGE:
            draw_message();
            break;
        case DISPLAY_PORTAL:
            draw_portal();
            break;
        case DISPLAY_SLEEP:
            /* Rien a dessiner, on attend juste */
            vTaskDelay(pdMS_TO_TICKS(1000));
            s_frame_count++;
            continue;
        }

        s_frame_count++;

        /* FPS adaptatif */
        int fps = (state == DISPLAY_THINKING) ? MIMI_DISP_FPS_ACTIVE : MIMI_DISP_FPS_IDLE;
        vTaskDelay(pdMS_TO_TICKS(1000 / fps));
    }
}

/* ---- API publique ---- */

esp_err_t display_ui_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    if (!s_mutex) return ESP_ERR_NO_MEM;

    xTaskCreatePinnedToCore(
        display_task, "display",
        MIMI_DISP_STACK, NULL,
        MIMI_DISP_PRIO, NULL, MIMI_DISP_CORE);

    ESP_LOGI(TAG, "Display UI initialized");
    return ESP_OK;
}

void display_ui_set_state(display_state_t state)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (s_state != state) {
        display_state_t old_state = s_state;
        s_state = state;
        s_frame_count = 0;

        if (state == DISPLAY_SLEEP) {
            display_hal_sleep();
        } else if (old_state == DISPLAY_SLEEP) {
            display_hal_wake();
        }
    }
    xSemaphoreGive(s_mutex);
}

void display_ui_set_message(const char *text)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    strncpy(s_message, text, sizeof(s_message) - 1);
    s_message[sizeof(s_message) - 1] = '\0';
    xSemaphoreGive(s_mutex);
}

void display_ui_set_status(bool wifi_ok, const char *ip)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_wifi_ok = wifi_ok;
    if (ip) {
        strncpy(s_ip, ip, sizeof(s_ip) - 1);
        s_ip[sizeof(s_ip) - 1] = '\0';
    }
    xSemaphoreGive(s_mutex);
}

display_state_t display_ui_get_state(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    display_state_t state = s_state;
    xSemaphoreGive(s_mutex);
    return state;
}

void display_ui_next_screen(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    switch (s_state) {
    case DISPLAY_IDLE:    s_state = DISPLAY_MESSAGE; break;
    case DISPLAY_MESSAGE: s_state = DISPLAY_IDLE;    break;
    default: break;  /* pas de cycle pendant thinking/portal/sleep */
    }
    s_frame_count = 0;
    xSemaphoreGive(s_mutex);
}
