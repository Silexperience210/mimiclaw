#include "display_ui.h"
#include "display_hal.h"
#include "lobster_sprite.h"
#include "mimi_config.h"
#include "ota/ota_manager.h"
#include "power/battery_monitor.h"
#ifdef MIMI_HAS_SERVOS
#include "hardware/sonar_radar.h"
#endif

#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"

static const char *TAG = "disp_ui";

/* ---- Font bitmap 5x7 compacte (ASCII 32-126) ---- */
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
#define COL_OCEAN    0x0A2F  /* bleu ocean fonce */
#define COL_SEAWEED  0x2C44  /* vert algue */
#define COL_BUBBLE   0x5D7F  /* bleu bulle clair */
#define COL_STAR     0xFFE0  /* jaune etoile */
#define COL_BANNER   0x18A3  /* gris fonce semi-transparent */
#define COL_CHARGE   0x47E0  /* vert charge */
#define COL_BOLT     0xFFE0  /* jaune eclair */
#define COL_BATT_BG  0x2104  /* gris fonce contour batterie */

/* ---- Etat interne ---- */
static display_state_t s_state = DISPLAY_IDLE;
static SemaphoreHandle_t s_mutex = NULL;
static char s_message[256] = {0};
static bool s_wifi_ok = false;
static char s_ip[20] = {0};
static uint32_t s_frame_count = 0;
static lobster_mood_t s_mood = MOOD_NEUTRAL;
static int64_t s_last_activity_us = 0;
static uint32_t s_msg_count = 0;

/* Notification banner */
static bool s_banner_active = false;
static int64_t s_banner_start_us = 0;
static char s_banner_text[64] = {0};
static int s_banner_y = 0;  /* position Y animee */

/* Transition */
static bool s_transitioning = false;
static int s_transition_frame = 0;
static display_state_t s_transition_target;

/* Screensaver aquarium */
static int s_lobster_x = 60;
static int s_lobster_dir = 1;  /* 1=droite, -1=gauche */
typedef struct {
    int x, y;
    int speed;  /* pixels par frame */
    int size;   /* rayon */
} bubble_t;
static bubble_t s_bubbles[MIMI_DISP_BUBBLE_COUNT];

/* Typewriter */
static int s_typewriter_pos = 0;

#ifdef MIMI_HAS_SERVOS
/* Etch-a-sketch */
#define ETCH_W  160
#define ETCH_H  85
static uint8_t *s_etch_canvas = NULL;  /* PSRAM, 160x85 pixels, 0=vide */
static int s_etch_cursor_x = ETCH_W / 2;
static int s_etch_cursor_y = ETCH_H / 2;
static bool s_etch_drawing = false;
static uint8_t s_etch_color_idx = 1;   /* index couleur courante 1-7 */

/* Palette etch-a-sketch (RGB565) */
static const uint16_t s_etch_palette[] = {
    0x0000,  /* 0: vide (noir) */
    0xFFFF,  /* 1: blanc */
    0xF800,  /* 2: rouge */
    0x07E0,  /* 3: vert */
    0x001F,  /* 4: bleu */
    0xFFE0,  /* 5: jaune */
    0x07FF,  /* 6: cyan */
    0xF81F,  /* 7: magenta */
};
#define ETCH_NUM_COLORS 7

/* Couleurs radar */
#define COL_RADAR_BG     0x0000  /* noir */
#define COL_RADAR_GRID   0x1082  /* gris tres fonce */
#define COL_RADAR_SWEEP  0x07E0  /* vert vif */
#define COL_RADAR_POINT  0xF800  /* rouge */
#define COL_RADAR_FADE1  0x8800  /* rouge sombre */
#define COL_RADAR_FADE2  0x4000  /* rouge tres sombre */
#define COL_RADAR_TEXT   0x07E0  /* vert */
#define COL_SENTINEL_ALERT 0xFBE0 /* orange vif */
#endif /* MIMI_HAS_SERVOS */

/* ---- Buffer PSRAM double ---- */
static uint16_t *s_framebuf = NULL;  /* framebuffer complet en PSRAM */
static uint16_t s_line_buf[MIMI_DISP_WIDTH * MIMI_DISP_BUF_LINES];

/* ---- Helpers PSRAM framebuffer ---- */

static void fb_clear(uint16_t color)
{
    if (s_framebuf) {
        for (int i = 0; i < MIMI_DISP_WIDTH * MIMI_DISP_HEIGHT; i++)
            s_framebuf[i] = color;
    }
}

static void fb_pixel(int x, int y, uint16_t color)
{
    if (s_framebuf && x >= 0 && x < MIMI_DISP_WIDTH && y >= 0 && y < MIMI_DISP_HEIGHT) {
        s_framebuf[y * MIMI_DISP_WIDTH + x] = color;
    }
}

static void fb_fill_rect(int x, int y, int w, int h, uint16_t color)
{
    for (int row = y; row < y + h && row < MIMI_DISP_HEIGHT; row++) {
        for (int col = x; col < x + w && col < MIMI_DISP_WIDTH; col++) {
            if (col >= 0 && row >= 0)
                s_framebuf[row * MIMI_DISP_WIDTH + col] = color;
        }
    }
}

static void fb_draw_char(int x, int y, char c, uint16_t color, int scale)
{
    if (c < 32 || c > 126) c = '?';
    const uint8_t *glyph = &font5x7[(c - 32) * 5];
    for (int row = 0; row < FONT_H; row++) {
        for (int sy = 0; sy < scale; sy++) {
            for (int col = 0; col < FONT_W; col++) {
                if (glyph[col] & (1 << row)) {
                    for (int sx = 0; sx < scale; sx++) {
                        fb_pixel(x + col * scale + sx, y + row * scale + sy, color);
                    }
                }
            }
        }
    }
}

static void fb_draw_string(int x, int y, const char *str, uint16_t color, int scale)
{
    int cx = x;
    while (*str) {
        if (*str == '\n') {
            y += (FONT_H + 2) * scale;
            cx = x;
            str++;
            continue;
        }
        fb_draw_char(cx, y, *str, color, scale);
        cx += (FONT_W + CHAR_SPACING) * scale;
        if (cx + FONT_W * scale > MIMI_DISP_WIDTH) {
            y += (FONT_H + 2) * scale;
            cx = x;
        }
        str++;
    }
}

/* Dessine un nombre limite de caracteres (typewriter) */
static void fb_draw_string_n(int x, int y, const char *str, int max_chars,
                              uint16_t color, int scale)
{
    int cx = x;
    int count = 0;
    while (*str && count < max_chars) {
        if (*str == '\n') {
            y += (FONT_H + 2) * scale;
            cx = x;
            str++;
            continue;
        }
        fb_draw_char(cx, y, *str, color, scale);
        cx += (FONT_W + CHAR_SPACING) * scale;
        if (cx + FONT_W * scale > MIMI_DISP_WIDTH) {
            y += (FONT_H + 2) * scale;
            cx = x;
        }
        str++;
        count++;
    }
}

static void fb_draw_sprite(int x, int y, const uint16_t *sprite, int w, int h, int scale)
{
    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
            uint16_t px = sprite[row * w + col];
            if (px == C_BG) continue;  /* transparent */
            for (int sy = 0; sy < scale; sy++) {
                for (int sx = 0; sx < scale; sx++) {
                    fb_pixel(x + col * scale + sx, y + row * scale + sy, px);
                }
            }
        }
    }
}

/* Flush framebuffer PSRAM vers l'ecran par bandes */
static void fb_flush(void)
{
    if (!s_framebuf) return;
    for (int y = 0; y < MIMI_DISP_HEIGHT; y += MIMI_DISP_BUF_LINES) {
        int h = MIMI_DISP_BUF_LINES;
        if (y + h > MIMI_DISP_HEIGHT) h = MIMI_DISP_HEIGHT - y;
        display_hal_flush(0, y, MIMI_DISP_WIDTH, h,
                          &s_framebuf[y * MIMI_DISP_WIDTH]);
    }
}

/* ---- Fallback sans PSRAM (ancienne methode) ---- */
static void fill_rect(int x, int y, int w, int h, uint16_t color)
{
    int buf_size = w * h;
    if (buf_size > MIMI_DISP_WIDTH * MIMI_DISP_BUF_LINES) {
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

/* ---- Emotions : modification des yeux sur le sprite ---- */

/* Positions des yeux dans le sprite 32x32 :
 * Oeil gauche  : lignes 11-13, colonnes 10-12
 * Oeil droit   : lignes 11-13, colonnes 17-19  */
#define EYE_L_X  10
#define EYE_R_X  17
#define EYE_Y    11

static void draw_mood_eyes(int base_x, int base_y, int scale)
{
    /* Dessine les yeux par-dessus le sprite selon l'humeur */
    switch (s_mood) {
    case MOOD_HAPPY:
        /* Yeux en demi-lune (arcs vers le haut) : U U */
        for (int e = 0; e < 2; e++) {
            int ex = base_x + (e == 0 ? EYE_L_X : EYE_R_X) * scale;
            int ey = base_y + EYE_Y * scale;
            /* Arc : pixels du bas de l'oeil seulement */
            for (int sx = 0; sx < 3 * scale; sx++) {
                fb_pixel(ex + sx, ey + 2 * scale, COL_TEXT);
                fb_pixel(ex + sx, ey + 2 * scale + 1, COL_TEXT);
            }
            fb_pixel(ex, ey + 1 * scale, COL_TEXT);
            fb_pixel(ex + 3 * scale - 1, ey + 1 * scale, COL_TEXT);
        }
        break;

    case MOOD_EXCITED:
        /* Yeux en etoile : * * */
        for (int e = 0; e < 2; e++) {
            int ex = base_x + (e == 0 ? EYE_L_X : EYE_R_X) * scale + 1 * scale;
            int ey = base_y + (EYE_Y + 1) * scale;
            /* Croix + diagonales */
            for (int d = -1; d <= 1; d++) {
                fb_pixel(ex + d * scale, ey, COL_STAR);
                fb_pixel(ex, ey + d * scale, COL_STAR);
                fb_pixel(ex + d * scale, ey + d * scale, COL_STAR);
                fb_pixel(ex + d * scale, ey - d * scale, COL_STAR);
            }
        }
        break;

    case MOOD_FOCUSED:
        /* Lunettes carrees autour des yeux */
        for (int e = 0; e < 2; e++) {
            int ex = base_x + (e == 0 ? EYE_L_X - 1 : EYE_R_X - 1) * scale;
            int ey = base_y + (EYE_Y - 1) * scale;
            int gw = 5 * scale;
            int gh = 5 * scale;
            /* Cadre */
            for (int i = 0; i < gw; i++) {
                fb_pixel(ex + i, ey, COL_TEXT);
                fb_pixel(ex + i, ey + gh - 1, COL_TEXT);
            }
            for (int i = 0; i < gh; i++) {
                fb_pixel(ex, ey + i, COL_TEXT);
                fb_pixel(ex + gw - 1, ey + i, COL_TEXT);
            }
        }
        /* Pont entre les lunettes */
        {
            int bridge_y = base_y + EYE_Y * scale;
            int bridge_x1 = base_x + (EYE_L_X + 3) * scale;
            int bridge_x2 = base_x + (EYE_R_X - 1) * scale;
            for (int i = bridge_x1; i <= bridge_x2; i++)
                fb_pixel(i, bridge_y, COL_TEXT);
        }
        break;

    case MOOD_SLEEPY:
        /* Yeux fermes + Zzz flottant */
        /* Les yeux fermes sont deja dans lobster_blink, on ajoute juste Zzz */
        {
            int zx = base_x + 24 * scale;
            int zy = base_y - 5 * scale;
            int offset = (s_frame_count / 3) % 4;
            fb_draw_char(zx, zy - offset * 2, 'Z', COL_DIM, scale > 1 ? scale - 1 : 1);
            fb_draw_char(zx + 4 * scale, zy - 8 - offset * 2, 'z', COL_DIM, scale > 1 ? scale - 1 : 1);
            fb_draw_char(zx + 2 * scale, zy - 16 - offset * 2, 'z', COL_DIM, 1);
        }
        break;

    case MOOD_PROUD:
        /* Petites etoiles/sparkles autour du lobster */
        {
            int sparkle_offsets[][2] = {
                {-8, 5}, {28, -3}, {-5, 20}, {30, 18}, {0, -6}, {25, 28}
            };
            int n_sparkles = 6;
            int visible = (s_frame_count / 2) % (n_sparkles + 1);
            for (int i = 0; i < visible && i < n_sparkles; i++) {
                int sx = base_x + sparkle_offsets[i][0] * scale;
                int sy = base_y + sparkle_offsets[i][1] * scale;
                fb_pixel(sx, sy, COL_STAR);
                fb_pixel(sx - 1, sy, COL_STAR);
                fb_pixel(sx + 1, sy, COL_STAR);
                fb_pixel(sx, sy - 1, COL_STAR);
                fb_pixel(sx, sy + 1, COL_STAR);
            }
        }
        break;

    default:
        break;
    }
}

/* ---- Bulles aquarium ---- */

static void bubbles_init(void)
{
    for (int i = 0; i < MIMI_DISP_BUBBLE_COUNT; i++) {
        s_bubbles[i].x = esp_random() % MIMI_DISP_WIDTH;
        s_bubbles[i].y = MIMI_DISP_HEIGHT + (esp_random() % 100);
        s_bubbles[i].speed = 1 + (esp_random() % 3);
        s_bubbles[i].size = 1 + (esp_random() % 3);
    }
}

static void bubbles_update(void)
{
    for (int i = 0; i < MIMI_DISP_BUBBLE_COUNT; i++) {
        s_bubbles[i].y -= s_bubbles[i].speed;
        /* Leger mouvement horizontal sinusoidal */
        s_bubbles[i].x += ((s_frame_count + i * 7) % 3 == 0) ? 1 : 0;
        s_bubbles[i].x -= ((s_frame_count + i * 5) % 3 == 0) ? 1 : 0;

        /* Reset si sorti de l'ecran */
        if (s_bubbles[i].y < -5) {
            s_bubbles[i].x = esp_random() % MIMI_DISP_WIDTH;
            s_bubbles[i].y = MIMI_DISP_HEIGHT + (esp_random() % 40);
            s_bubbles[i].speed = 1 + (esp_random() % 3);
            s_bubbles[i].size = 1 + (esp_random() % 3);
        }
    }
}

static void bubbles_draw(void)
{
    for (int i = 0; i < MIMI_DISP_BUBBLE_COUNT; i++) {
        int bx = s_bubbles[i].x;
        int by = s_bubbles[i].y;
        int r = s_bubbles[i].size;
        /* Cercle simple */
        for (int dy = -r; dy <= r; dy++) {
            for (int dx = -r; dx <= r; dx++) {
                if (dx * dx + dy * dy <= r * r) {
                    fb_pixel(bx + dx, by + dy, COL_BUBBLE);
                }
            }
        }
        /* Reflet */
        fb_pixel(bx - r / 2, by - r / 2, COL_TEXT);
    }
}

/* Algues au fond de l'ecran */
static void draw_seaweed(void)
{
    /* 5 algues reparties sur la largeur (320px paysage) */
    int positions[] = {20, 80, 150, 220, 280};
    for (int a = 0; a < 5; a++) {
        int base_x = positions[a];
        int height = 20 + (a % 3) * 8;
        int sway = ((s_frame_count + a * 3) / 4) % 5 - 2;

        for (int y = 0; y < height; y++) {
            int x = base_x + (y * sway) / height;
            int w = 3 - (y * 2 / height);
            if (w < 1) w = 1;
            fb_fill_rect(x, MIMI_DISP_HEIGHT - 10 - y, w, 1, COL_SEAWEED);
        }
    }
    /* Fond sableux */
    fb_fill_rect(0, MIMI_DISP_HEIGHT - 10, MIMI_DISP_WIDTH, 10, 0x4208);
}

/* ---- Ecrans ---- */

static void draw_status_bar(void)
{
    fb_fill_rect(0, 0, MIMI_DISP_WIDTH, 12, COL_BG);
    uint16_t wifi_col = s_wifi_ok ? COL_GREEN : COL_RED;
    fb_draw_char(2, 2, s_wifi_ok ? 'W' : '!', wifi_col, 1);
    if (s_wifi_ok && s_ip[0]) {
        fb_draw_string(12, 2, s_ip, COL_DIM, 1);
    }
    /* Compteur messages en haut a droite */
    if (s_msg_count > 0) {
        char cnt[12];
        snprintf(cnt, sizeof(cnt), "%lu", (unsigned long)s_msg_count);
        int len = strlen(cnt);
        fb_draw_string(MIMI_DISP_WIDTH - len * 6 - 2, 2, cnt, COL_ACCENT, 1);
    }
}

static void draw_idle(void)
{
    fb_clear(COL_BG);
    draw_status_bar();

    /* Paysage : lobster a gauche (scale 3 = 96x96), texte a droite */
    int sprite_scale = 3;
    int sx = 10;
    int sy = (MIMI_DISP_HEIGHT - LOBSTER_H * sprite_scale) / 2 + 6;

    bool use_blink = false;
    if (s_mood == MOOD_SLEEPY) {
        use_blink = true;
    } else if (s_mood == MOOD_NEUTRAL) {
        use_blink = (s_frame_count % (MIMI_DISP_FPS_IDLE * 4) < 1);
    }

    const uint16_t *frame = use_blink ? lobster_blink : lobster_idle;
    fb_draw_sprite(sx, sy, frame, LOBSTER_W, LOBSTER_H, sprite_scale);
    draw_mood_eyes(sx, sy, sprite_scale);

    /* Titre a droite du lobster */
    int tx = sx + LOBSTER_W * sprite_scale + 15;
    fb_draw_string(tx, 25, "LilyClaw", COL_ACCENT, 2);

    /* Sous-titre selon humeur */
    const char *sub = "AI Assistant";
    switch (s_mood) {
    case MOOD_HAPPY:   sub = "I'm happy!"; break;
    case MOOD_SLEEPY:  sub = "Sleepy..."; break;
    case MOOD_EXCITED: sub = "New message!"; break;
    case MOOD_PROUD:   sub = "Nailed it!"; break;
    default: break;
    }
    fb_draw_string(tx, 52, sub, COL_DIM, 1);

    /* Ligne decorative */
    fb_fill_rect(tx, 65, 120, 1, COL_ACCENT);
    fb_draw_string(tx, 72, "ESP32-S3 AI", COL_DIM, 1);

    /* Version en bas a droite */
    char ver_str[16];
    snprintf(ver_str, sizeof(ver_str), "v%s", MIMI_FW_VERSION);
    int ver_len = strlen(ver_str);
    fb_draw_string(MIMI_DISP_WIDTH - ver_len * 6 - 4, MIMI_DISP_HEIGHT - 10,
                   ver_str, COL_DIM, 1);
}

static void draw_thinking(void)
{
    fb_clear(COL_BG);
    draw_status_bar();

    /* Paysage : lobster a gauche (scale 3 = 96x96), thinking a droite */
    int sprite_scale = 3;
    int sx = 10;
    int sy = (MIMI_DISP_HEIGHT - LOBSTER_H * sprite_scale) / 2 + 6;

    int anim = (s_frame_count / 4) % 2;
    const uint16_t *frame = anim ? lobster_blink : lobster_idle;
    fb_draw_sprite(sx, sy, frame, LOBSTER_W, LOBSTER_H, sprite_scale);

    s_mood = MOOD_FOCUSED;
    draw_mood_eyes(sx, sy, sprite_scale);

    /* Texte a droite */
    int tx = sx + LOBSTER_W * sprite_scale + 15;
    int dots = (s_frame_count / 5) % 4;
    char think_text[16] = "Thinking";
    for (int i = 0; i < dots; i++) strcat(think_text, ".");
    fb_draw_string(tx, 35, think_text, COL_THINK, 2);

    /* Barre de progression */
    int bar_y = 68;
    int bar_w = MIMI_DISP_WIDTH - tx - 10;
    fb_fill_rect(tx, bar_y, bar_w, 4, 0x2104);
    int progress = (s_frame_count * 3) % bar_w;
    int seg_w = 40;
    if (progress + seg_w > bar_w) seg_w = bar_w - progress;
    fb_fill_rect(tx + progress, bar_y, seg_w, 4, COL_THINK);
}

static void draw_message(void)
{
    fb_clear(COL_BG);
    draw_status_bar();

    fb_draw_string(8, 18, "Last message:", COL_ACCENT, 1);
    fb_fill_rect(8, 28, 200, 1, COL_ACCENT);

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (s_message[0]) {
        int max_chars = s_typewriter_pos;
        if (max_chars > (int)strlen(s_message)) max_chars = strlen(s_message);
        fb_draw_string_n(8, 34, s_message, max_chars, COL_TEXT, 1);
        if (s_typewriter_pos <= (int)strlen(s_message)) {
            s_typewriter_pos += 2;
        }
    } else {
        fb_draw_string(8, 34, "(aucun message)", COL_DIM, 1);
    }
    xSemaphoreGive(s_mutex);
}

static void draw_portal(void)
{
    fb_clear(COL_BG);

    /* Paysage : lobster a gauche (scale 3), infos a droite */
    int sprite_scale = 3;
    int sx = 10;
    int sy = (MIMI_DISP_HEIGHT - LOBSTER_H * sprite_scale) / 2 + 6;
    fb_draw_sprite(sx, sy, lobster_idle, LOBSTER_W, LOBSTER_H, sprite_scale);

    int tx = sx + LOBSTER_W * sprite_scale + 15;
    fb_draw_string(tx, 10, "Setup Mode", COL_ACCENT, 2);
    fb_draw_string(tx, 35, "WiFi:", COL_DIM, 1);
    fb_draw_string(tx + 36, 35, MIMI_PORTAL_AP_SSID, COL_TEXT, 1);
    fb_draw_string(tx, 50, "Pass:", COL_DIM, 1);
    fb_draw_string(tx + 36, 50, MIMI_PORTAL_AP_PASS, COL_TEXT, 1);
    fb_fill_rect(tx, 63, MIMI_DISP_WIDTH - tx - 5, 1, COL_DIM);
    fb_draw_string(tx, 70, "Open:", COL_DIM, 1);
    fb_draw_string(tx, 85, "192.168.4.1", COL_ACCENT, 2);
}

static void draw_screensaver(void)
{
    fb_clear(COL_OCEAN);

    draw_seaweed();
    bubbles_update();
    bubbles_draw();

    /* Lobster qui marche — scale 2 = 64x64, hauteur dispo ~150px */
    int sprite_scale = 2;
    int ly = MIMI_DISP_HEIGHT - 12 - LOBSTER_H * sprite_scale;

    s_lobster_x += s_lobster_dir * 2;
    if (s_lobster_x > MIMI_DISP_WIDTH - LOBSTER_W * sprite_scale - 5) {
        s_lobster_dir = -1;
    } else if (s_lobster_x < 5) {
        s_lobster_dir = 1;
    }

    bool walk_anim = (s_frame_count / 3) % 2;
    const uint16_t *frame = walk_anim ? lobster_blink : lobster_idle;
    fb_draw_sprite(s_lobster_x, ly, frame, LOBSTER_W, LOBSTER_H, sprite_scale);
}

/* ---- Radar sonar ---- */
#ifdef MIMI_HAS_SERVOS

/* Dessine une ligne du centre vers (x1,y1) */
static void fb_draw_line(int x0, int y0, int x1, int y1, uint16_t color)
{
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    for (int i = 0; i < 400; i++) { /* securite anti boucle infinie */
        fb_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx)  { err += dx; y0 += sy; }
    }
}

static void draw_radar(void)
{
    fb_clear(COL_RADAR_BG);

    /* Centre du radar : bas-centre de l'ecran */
    int cx = MIMI_DISP_WIDTH / 2;   /* 160 */
    int cy = MIMI_DISP_HEIGHT - 5;  /* 165 */

    /* Echelle : max distance → max rayon pixel */
    float scale = (float)(MIMI_DISP_HEIGHT - 20) / RADAR_MAX_DIST_CM; /* ~0.5 px/cm */

    /* Arcs de distance (50cm, 150cm, 300cm) */
    int dist_rings[] = {50, 150, 300};
    for (int r = 0; r < 3; r++) {
        int radius = (int)(dist_rings[r] * scale);
        /* Dessine l'arc de RADAR_SWEEP_MIN a RADAR_SWEEP_MAX */
        for (int a = RADAR_SWEEP_MIN; a <= RADAR_SWEEP_MAX; a++) {
            float rad = (float)a * 3.14159f / 180.0f;
            int px = cx + (int)(radius * cosf(rad));
            int py = cy - (int)(radius * sinf(rad));
            fb_pixel(px, py, COL_RADAR_GRID);
        }
        /* Label distance */
        char dist_label[16];
        snprintf(dist_label, sizeof(dist_label), "%dm", dist_rings[r] / 100);
        fb_draw_string(cx + radius - 10, cy - 8, dist_label, COL_RADAR_GRID, 1);
    }

    /* Lignes radiales tous les 15° */
    for (int a = RADAR_SWEEP_MIN; a <= RADAR_SWEEP_MAX; a += 15) {
        float rad = (float)a * 3.14159f / 180.0f;
        int max_r = (int)(RADAR_MAX_DIST_CM * scale);
        int ex = cx + (int)(max_r * cosf(rad));
        int ey = cy - (int)(max_r * sinf(rad));
        fb_draw_line(cx, cy, ex, ey, COL_RADAR_GRID);
    }

    /* Points detectes — sweeps historiques (afterglow) */
    for (int sweep = RADAR_NUM_SWEEPS - 1; sweep >= 0; sweep--) {
        const radar_point_t *pts = sonar_radar_get_sweep(sweep);
        if (!pts) continue;

        uint16_t color;
        int dot_size;
        switch (sweep) {
        case 0:  color = COL_RADAR_POINT; dot_size = 3; break;
        case 1:  color = COL_RADAR_FADE1; dot_size = 2; break;
        default: color = COL_RADAR_FADE2; dot_size = 1; break;
        }

        for (int i = 0; i < RADAR_POINTS; i++) {
            int d = pts[i].distance;
            if (d <= 0 || d > RADAR_MAX_DIST_CM) continue;

            uint8_t angle = sonar_radar_index_to_angle(i);
            float rad = (float)angle * 3.14159f / 180.0f;
            int r = (int)(d * scale);
            int px = cx + (int)(r * cosf(rad));
            int py = cy - (int)(r * sinf(rad));

            /* Point avec taille variable */
            for (int dy = -dot_size; dy <= dot_size; dy++) {
                for (int dx = -dot_size; dx <= dot_size; dx++) {
                    if (dx * dx + dy * dy <= dot_size * dot_size) {
                        fb_pixel(px + dx, py + dy, color);
                    }
                }
            }
        }
    }

    /* Ligne de balayage (sweep line) */
    int cur_idx = sonar_radar_get_current_index();
    uint8_t cur_angle = sonar_radar_index_to_angle(cur_idx);
    float sweep_rad = (float)cur_angle * 3.14159f / 180.0f;
    int sweep_len = (int)(RADAR_MAX_DIST_CM * scale);
    int sweep_ex = cx + (int)(sweep_len * cosf(sweep_rad));
    int sweep_ey = cy - (int)(sweep_len * sinf(sweep_rad));
    fb_draw_line(cx, cy, sweep_ex, sweep_ey, COL_RADAR_SWEEP);

    /* Point central (le lobster) */
    for (int dy = -2; dy <= 2; dy++) {
        for (int dx = -2; dx <= 2; dx++) {
            fb_pixel(cx + dx, cy + dy, COL_RADAR_SWEEP);
        }
    }

    /* Texte HUD */
    radar_mode_t mode = sonar_radar_get_mode();
    if (mode == RADAR_SENTINEL) {
        fb_draw_string(2, 2, "SENTINEL", COL_SENTINEL_ALERT, 1);

        /* Clignotement si intrusion */
        sentinel_alert_t alert = sonar_radar_check_intrusion();
        if (alert.detected && (s_frame_count / 3) % 2) {
            fb_draw_string(60, 2, "ALERTE!", COL_SENTINEL_ALERT, 2);
        }
    } else {
        fb_draw_string(2, 2, "SONAR", COL_RADAR_TEXT, 1);
    }

    /* Angle courant */
    char angle_str[16];
    snprintf(angle_str, sizeof(angle_str), "%d deg", cur_angle);
    fb_draw_string(MIMI_DISP_WIDTH - 50, 2, angle_str, COL_RADAR_TEXT, 1);

    /* Distance la plus proche */
    const radar_point_t *current = sonar_radar_get_sweep(0);
    if (current) {
        int closest = 9999;
        for (int i = 0; i < RADAR_POINTS; i++) {
            if (current[i].distance > 0 && current[i].distance < closest) {
                closest = current[i].distance;
            }
        }
        if (closest < 9999) {
            char dist_str[24];
            snprintf(dist_str, sizeof(dist_str), "Near:%dcm", closest);
            fb_draw_string(2, MIMI_DISP_HEIGHT - 10, dist_str, COL_RADAR_TEXT, 1);
        }
    }
}
#endif /* MIMI_HAS_SERVOS */

/* ---- Etch-a-sketch sans contact ---- */
#ifdef MIMI_HAS_SERVOS

static void draw_etchasketch(void)
{
    if (!s_etch_canvas) return;

    fb_clear(COL_BG);

    /* Dessiner le canvas a scale 2x */
    for (int y = 0; y < ETCH_H; y++) {
        for (int x = 0; x < ETCH_W; x++) {
            uint8_t c = s_etch_canvas[y * ETCH_W + x];
            if (c > 0 && c <= ETCH_NUM_COLORS) {
                uint16_t color = s_etch_palette[c];
                /* Scale 2x */
                fb_pixel(x * 2,     y * 2,     color);
                fb_pixel(x * 2 + 1, y * 2,     color);
                fb_pixel(x * 2,     y * 2 + 1, color);
                fb_pixel(x * 2 + 1, y * 2 + 1, color);
            }
        }
    }

    /* Curseur (clignotant) */
    if ((s_frame_count / 3) % 2) {
        uint16_t cur_col = s_etch_drawing ? s_etch_palette[s_etch_color_idx] : COL_DIM;
        int cx = s_etch_cursor_x * 2;
        int cy = s_etch_cursor_y * 2;
        /* Croix */
        for (int d = -3; d <= 3; d++) {
            fb_pixel(cx + d, cy, cur_col);
            fb_pixel(cx, cy + d, cur_col);
        }
    }

    /* Si on dessine, marquer le pixel */
    if (s_etch_drawing) {
        int ex = s_etch_cursor_x;
        int ey = s_etch_cursor_y;
        if (ex >= 0 && ex < ETCH_W && ey >= 0 && ey < ETCH_H) {
            s_etch_canvas[ey * ETCH_W + ex] = s_etch_color_idx;
            /* Epaisseur 2 pour trait plus visible */
            if (ex + 1 < ETCH_W) s_etch_canvas[ey * ETCH_W + ex + 1] = s_etch_color_idx;
            if (ey + 1 < ETCH_H) s_etch_canvas[(ey + 1) * ETCH_W + ex] = s_etch_color_idx;
        }
    }

    /* HUD en bas */
    fb_fill_rect(0, MIMI_DISP_HEIGHT - 12, MIMI_DISP_WIDTH, 12, 0x1082);
    fb_draw_string(2, MIMI_DISP_HEIGHT - 10, "ETCH", COL_ACCENT, 1);

    /* Indicateur couleur courante */
    fb_fill_rect(40, MIMI_DISP_HEIGHT - 10, 8, 8, s_etch_palette[s_etch_color_idx]);

    /* Instructions */
    const char *hint = s_etch_drawing ? "DRAWING" : "MOVE";
    fb_draw_string(55, MIMI_DISP_HEIGHT - 10, hint, COL_DIM, 1);

    /* Coordonnees */
    char pos[16];
    snprintf(pos, sizeof(pos), "%d,%d", s_etch_cursor_x, s_etch_cursor_y);
    fb_draw_string(MIMI_DISP_WIDTH - 40, MIMI_DISP_HEIGHT - 10, pos, COL_DIM, 1);
}
#endif /* MIMI_HAS_SERVOS */

/* ---- Animation charge batterie ---- */

static void draw_charging(void)
{
    fb_clear(COL_BG);

    int percent = battery_get_percent();
    int voltage = battery_get_voltage_mv();

    /* Icone batterie centree — dimensions */
    int batt_w = 120;   /* largeur corps batterie */
    int batt_h = 60;    /* hauteur corps batterie */
    int batt_x = (MIMI_DISP_WIDTH - batt_w) / 2;
    int batt_y = 25;
    int tip_w = 8;      /* borne positive */
    int tip_h = 24;

    /* Contour batterie (blanc) */
    /* Haut */
    fb_fill_rect(batt_x, batt_y, batt_w, 3, COL_TEXT);
    /* Bas */
    fb_fill_rect(batt_x, batt_y + batt_h - 3, batt_w, 3, COL_TEXT);
    /* Gauche */
    fb_fill_rect(batt_x, batt_y, 3, batt_h, COL_TEXT);
    /* Droite */
    fb_fill_rect(batt_x + batt_w - 3, batt_y, 3, batt_h, COL_TEXT);
    /* Borne positive (a droite) */
    fb_fill_rect(batt_x + batt_w, batt_y + (batt_h - tip_h) / 2,
                 tip_w, tip_h, COL_TEXT);

    /* Interieur : fond sombre */
    int inner_x = batt_x + 5;
    int inner_y = batt_y + 5;
    int inner_w = batt_w - 10;
    int inner_h = batt_h - 10;
    fb_fill_rect(inner_x, inner_y, inner_w, inner_h, COL_BATT_BG);

    /* Remplissage anime — pulse doux pour simuler la charge */
    int base_fill = percent * inner_w / 100;

    /* Animation pulse : le remplissage "respire" un peu */
    int pulse = 0;
    if (percent < 100) {
        /* Sinusoide lente : ±5 pixels */
        int phase = (s_frame_count * 3) % 60;
        if (phase < 30)
            pulse = phase / 6;
        else
            pulse = (60 - phase) / 6;
    }
    int fill_w = base_fill + pulse;
    if (fill_w > inner_w) fill_w = inner_w;
    if (fill_w < 0) fill_w = 0;

    /* Couleur de remplissage selon niveau */
    uint16_t fill_col;
    if (percent < 20)
        fill_col = COL_RED;       /* critique = rouge */
    else if (percent < 50)
        fill_col = COL_ACCENT;    /* moyen = orange */
    else
        fill_col = COL_CHARGE;    /* bon = vert */

    /* Dessiner le remplissage */
    if (fill_w > 0) {
        fb_fill_rect(inner_x, inner_y, fill_w, inner_h, fill_col);
    }

    /* Segments de separation (style batterie) */
    for (int i = 1; i < 4; i++) {
        int seg_x = inner_x + i * inner_w / 4;
        fb_fill_rect(seg_x, inner_y, 1, inner_h, COL_BATT_BG);
    }

    /* Eclair anime au centre de la batterie (clignotant) */
    if ((s_frame_count / 4) % 3 != 0) {  /* visible 2/3 du temps */
        int lx = batt_x + batt_w / 2 - 6;
        int ly = batt_y + 8;
        /* Eclair simplifie en forme de Z */
        /*   ##
         *  ##
         * ####
         *   ##
         *  ##
         */
        fb_fill_rect(lx + 4, ly,      8, 3, COL_BOLT);
        fb_fill_rect(lx + 2, ly + 3,  8, 3, COL_BOLT);
        fb_fill_rect(lx,     ly + 6,  12, 3, COL_BOLT);
        fb_fill_rect(lx + 2, ly + 9,  8, 3, COL_BOLT);
        fb_fill_rect(lx,     ly + 12, 8, 3, COL_BOLT);

        /* Contour sombre pour contraste */
        fb_fill_rect(lx + 3, ly - 1,   10, 1, COL_BG);
        fb_fill_rect(lx - 1, ly + 15,  10, 1, COL_BG);
    }

    /* Pourcentage en gros sous la batterie */
    char pct_str[16];
    snprintf(pct_str, sizeof(pct_str), "%d%%", percent);
    int pct_len = 0;
    const char *p = pct_str;
    while (*p) { pct_len++; p++; }
    int pct_x = (MIMI_DISP_WIDTH - pct_len * 12) / 2;  /* scale 2 = 12px/char */
    fb_draw_string(pct_x, batt_y + batt_h + 12, pct_str, COL_TEXT, 2);

    /* Tension en petit sous le pourcentage */
    char volt_str[24];
    snprintf(volt_str, sizeof(volt_str), "%d.%02dV", voltage / 1000, (voltage % 1000) / 10);
    int volt_len = 0;
    const char *v = volt_str;
    while (*v) { volt_len++; v++; }
    int volt_x = (MIMI_DISP_WIDTH - volt_len * 6) / 2;
    fb_draw_string(volt_x, batt_y + batt_h + 34, volt_str, COL_DIM, 1);

    /* Texte "Charge..." anime en haut */
    int dots = (s_frame_count / 5) % 4;
    char charge_text[16] = "Charge";
    for (int i = 0; i < dots; i++) strcat(charge_text, ".");
    int ct_len = 0;
    const char *c = charge_text;
    while (*c) { ct_len++; c++; }
    int ct_x = (MIMI_DISP_WIDTH - ct_len * 12) / 2;
    fb_draw_string(ct_x, 6, charge_text, COL_CHARGE, 2);

    /* Petites particules qui montent (effet charge) */
    for (int i = 0; i < 5; i++) {
        int px = inner_x + fill_w - 3 + (i * 7) % 12;
        int py_base = inner_y + inner_h;
        int py = py_base - ((s_frame_count * 2 + i * 8) % (inner_h + 15));
        if (py >= inner_y && py < inner_y + inner_h && fill_w > 10) {
            fb_pixel(px, py, COL_TEXT);
            fb_pixel(px + 1, py, COL_TEXT);
            fb_pixel(px, py - 1, COL_TEXT);
        }
    }
}

/* ---- Notification banner (slide depuis le bas) ---- */

static void draw_banner(void)
{
    if (!s_banner_active) return;

    int64_t elapsed_us = esp_timer_get_time() - s_banner_start_us;
    int elapsed_ms = (int)(elapsed_us / 1000);

    /* Animation slide up (100ms) */
    int target_y = MIMI_DISP_HEIGHT - MIMI_DISP_BANNER_H;
    if (elapsed_ms < 100) {
        /* Slide up */
        s_banner_y = MIMI_DISP_HEIGHT - (MIMI_DISP_BANNER_H * elapsed_ms / 100);
    } else if (elapsed_ms > MIMI_DISP_BANNER_MS - 100) {
        /* Slide down pour disparaitre */
        int fade_ms = elapsed_ms - (MIMI_DISP_BANNER_MS - 100);
        s_banner_y = target_y + (MIMI_DISP_BANNER_H * fade_ms / 100);
    } else {
        s_banner_y = target_y;
    }

    /* Auto-dismiss */
    if (elapsed_ms >= MIMI_DISP_BANNER_MS) {
        s_banner_active = false;
        return;
    }

    /* Dessiner le banner */
    if (s_banner_y < MIMI_DISP_HEIGHT) {
        fb_fill_rect(0, s_banner_y, MIMI_DISP_WIDTH, MIMI_DISP_BANNER_H, COL_BANNER);
        /* Ligne de separation en haut du banner */
        fb_fill_rect(0, s_banner_y, MIMI_DISP_WIDTH, 1, COL_ACCENT);
        /* Icone message */
        fb_draw_char(6, s_banner_y + 6, '>', COL_ACCENT, 2);
        /* Texte tronque */
        fb_draw_string(26, s_banner_y + 8, s_banner_text, COL_TEXT, 1);
        /* Compteur en bas */
        char cnt_str[16];
        snprintf(cnt_str, sizeof(cnt_str), "#%lu", (unsigned long)s_msg_count);
        fb_draw_string(MIMI_DISP_WIDTH - 30, s_banner_y + MIMI_DISP_BANNER_H - 12,
                       cnt_str, COL_DIM, 1);
    }
}

/* ---- Transition wipe horizontal ---- */

static void draw_transition(void)
{
    /* Wipe depuis la gauche : bande noire qui traverse l'ecran */
    int wipe_x = (s_transition_frame * MIMI_DISP_WIDTH) / MIMI_DISP_TRANSITION_FRAMES;
    int wipe_w = MIMI_DISP_WIDTH / MIMI_DISP_TRANSITION_FRAMES + 5;

    fb_fill_rect(wipe_x - wipe_w, 0, wipe_w, MIMI_DISP_HEIGHT, COL_BG);

    s_transition_frame++;
    if (s_transition_frame >= MIMI_DISP_TRANSITION_FRAMES) {
        s_transitioning = false;
        s_state = s_transition_target;
        s_frame_count = 0;
        s_typewriter_pos = 0;
    }
}

/* ---- Boot animation cinematique ---- */

static void draw_boot_animation(void)
{
    if (!s_framebuf) {
        fill_rect(0, 0, MIMI_DISP_WIDTH, MIMI_DISP_HEIGHT, COL_BG);
        return;
    }

    /* Paysage : lobster a gauche (scale 3), texte a droite */
    int sprite_scale = 3;
    int target_x = 10;
    int target_y = (MIMI_DISP_HEIGHT - LOBSTER_H * sprite_scale) / 2 + 6;
    int tx = target_x + LOBSTER_W * sprite_scale + 15;

    /* Phase 1 : lobster glisse depuis la gauche (12 frames) */
    for (int f = 0; f < 12; f++) {
        fb_clear(COL_BG);
        int slide_x = -LOBSTER_W * sprite_scale + (target_x + LOBSTER_W * sprite_scale) * f / 11;
        if (f >= 10) slide_x = target_x + (11 - f) * 3;  /* petit rebond */
        fb_draw_sprite(slide_x, target_y, lobster_idle, LOBSTER_W, LOBSTER_H, sprite_scale);
        fb_flush();
        vTaskDelay(pdMS_TO_TICKS(35));
    }

    /* Phase 2 : texte lettre par lettre */
    fb_clear(COL_BG);
    fb_draw_sprite(target_x, target_y, lobster_idle, LOBSTER_W, LOBSTER_H, sprite_scale);
    fb_flush();

    const char *title = "LilyClaw";
    for (int i = 0; title[i]; i++) {
        fb_draw_char(tx + i * 12, 40, title[i], COL_ACCENT, 2);
        fb_flush();
        vTaskDelay(pdMS_TO_TICKS(70));
    }

    /* Phase 3 : clin d'oeil */
    vTaskDelay(pdMS_TO_TICKS(250));
    fb_clear(COL_BG);
    fb_draw_sprite(target_x, target_y, lobster_blink, LOBSTER_W, LOBSTER_H, sprite_scale);
    fb_draw_string(tx, 40, "LilyClaw", COL_ACCENT, 2);
    fb_flush();
    vTaskDelay(pdMS_TO_TICKS(200));

    fb_clear(COL_BG);
    fb_draw_sprite(target_x, target_y, lobster_idle, LOBSTER_W, LOBSTER_H, sprite_scale);
    fb_draw_string(tx, 40, "LilyClaw", COL_ACCENT, 2);
    fb_draw_string(tx, 65, "Ready!", COL_GREEN, 1);
    fb_flush();
    vTaskDelay(pdMS_TO_TICKS(600));
}

/* ---- Mise a jour automatique de l'humeur ---- */

static void update_mood_auto(void)
{
    if (s_state == DISPLAY_THINKING) {
        s_mood = MOOD_FOCUSED;
        return;
    }
    if (s_state != DISPLAY_IDLE && s_state != DISPLAY_SCREENSAVER) return;

    int64_t now = esp_timer_get_time();
    int64_t idle_sec = (now - s_last_activity_us) / 1000000;

    /* Hierarchie : excited > proud > happy > sleepy > neutral */
    if (s_mood == MOOD_EXCITED && idle_sec < 10) return;  /* garde excited 10s */
    if (s_mood == MOOD_PROUD && idle_sec < 15) return;    /* garde proud 15s */

    if (idle_sec > 45) {
        s_mood = MOOD_SLEEPY;
    } else if (s_msg_count >= 5) {
        s_mood = MOOD_HAPPY;
    } else {
        s_mood = MOOD_NEUTRAL;
    }
}

/* ---- Task principale d'affichage ---- */

static void display_task(void *arg)
{
    ESP_LOGI(TAG, "Display task started");

    /* Allocation framebuffer PSRAM */
    s_framebuf = heap_caps_malloc(MIMI_DISP_WIDTH * MIMI_DISP_HEIGHT * sizeof(uint16_t),
                                   MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (s_framebuf) {
        ESP_LOGI(TAG, "PSRAM framebuffer OK (%d bytes)", MIMI_DISP_WIDTH * MIMI_DISP_HEIGHT * 2);
    } else {
        ESP_LOGW(TAG, "Pas de PSRAM, framebuffer en bande");
    }

    /* Init bulles */
    bubbles_init();
    s_last_activity_us = esp_timer_get_time();

#ifdef MIMI_HAS_SERVOS
    /* Alloc canvas etch-a-sketch en PSRAM */
    s_etch_canvas = heap_caps_calloc(1, ETCH_W * ETCH_H, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (s_etch_canvas) {
        ESP_LOGI(TAG, "Etch-a-sketch canvas OK (%d bytes)", ETCH_W * ETCH_H);
    }
#endif

    /* Boot animation cinematique */
    draw_boot_animation();

    while (1) {
        display_state_t state;
        xSemaphoreTake(s_mutex, portMAX_DELAY);
        state = s_state;
        xSemaphoreGive(s_mutex);

        /* Mise a jour humeur automatique */
        update_mood_auto();

        /* Check screensaver : idle depuis > 60s (pas en charge) */
        if (state == DISPLAY_IDLE && !battery_is_charging()) {
            int64_t idle_ms = (esp_timer_get_time() - s_last_activity_us) / 1000;
            if (idle_ms > MIMI_DISP_SCREENSAVER_MS) {
                xSemaphoreTake(s_mutex, portMAX_DELAY);
                s_state = DISPLAY_SCREENSAVER;
                state = DISPLAY_SCREENSAVER;
                xSemaphoreGive(s_mutex);
            }
        }

        if (!s_framebuf) {
            /* Fallback sans PSRAM — juste idle basique */
            vTaskDelay(pdMS_TO_TICKS(500));
            s_frame_count++;
            continue;
        }

        /* Transition en cours ? */
        if (s_transitioning) {
            draw_transition();
            fb_flush();
            vTaskDelay(pdMS_TO_TICKS(30));
            continue;
        }

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
        case DISPLAY_SCREENSAVER:
            draw_screensaver();
            break;
#ifdef MIMI_HAS_SERVOS
        case DISPLAY_RADAR:
            draw_radar();
            break;
        case DISPLAY_ETCHASKETCH:
            draw_etchasketch();
            break;
#endif
        case DISPLAY_CHARGING:
            draw_charging();
            break;
        case DISPLAY_SLEEP:
            vTaskDelay(pdMS_TO_TICKS(1000));
            s_frame_count++;
            continue;
        default:
            break;
        }

        /* Banner par-dessus tout */
        draw_banner();

        fb_flush();
        s_frame_count++;

        /* FPS adaptatif */
        int fps = MIMI_DISP_FPS_IDLE;
        if (state == DISPLAY_THINKING || s_banner_active || state == DISPLAY_RADAR)
            fps = MIMI_DISP_FPS_ACTIVE;
        else if (state == DISPLAY_SCREENSAVER)
            fps = MIMI_DISP_FPS_SCREENSAVER;
        else if (state == DISPLAY_ETCHASKETCH)
            fps = MIMI_DISP_FPS_ACTIVE;
        else if (state == DISPLAY_CHARGING)
            fps = MIMI_DISP_FPS_SCREENSAVER;  /* 6fps suffisant pour l'animation */

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

        /* Transition wipe sauf vers/depuis sleep, radar, etch, charge */
        if (state != DISPLAY_SLEEP && old_state != DISPLAY_SLEEP
            && state != DISPLAY_SCREENSAVER && old_state != DISPLAY_SCREENSAVER
            && state != DISPLAY_RADAR && old_state != DISPLAY_RADAR
            && state != DISPLAY_ETCHASKETCH && old_state != DISPLAY_ETCHASKETCH
            && state != DISPLAY_CHARGING && old_state != DISPLAY_CHARGING) {
            s_transitioning = true;
            s_transition_frame = 0;
            s_transition_target = state;
        } else {
            s_state = state;
            s_frame_count = 0;
            s_typewriter_pos = 0;
        }

        if (state == DISPLAY_SLEEP) {
            s_state = DISPLAY_SLEEP;
            display_hal_sleep();
        } else if (old_state == DISPLAY_SLEEP) {
            display_hal_wake();
        }

        s_last_activity_us = esp_timer_get_time();
    }
    xSemaphoreGive(s_mutex);
}

void display_ui_set_message(const char *text)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    strncpy(s_message, text, sizeof(s_message) - 1);
    s_message[sizeof(s_message) - 1] = '\0';
    s_typewriter_pos = 0;
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
    case DISPLAY_IDLE:        s_state = DISPLAY_MESSAGE;     break;
    case DISPLAY_MESSAGE:     s_state = DISPLAY_IDLE;        break;
    case DISPLAY_SCREENSAVER: s_state = DISPLAY_IDLE;        break;
    case DISPLAY_RADAR:       s_state = DISPLAY_IDLE;        break;
    case DISPLAY_ETCHASKETCH: s_state = DISPLAY_IDLE;        break;
    case DISPLAY_CHARGING:    s_state = DISPLAY_CHARGING;    break; /* reste en charge */
    default: break;
    }
    s_frame_count = 0;
    s_typewriter_pos = 0;
    s_last_activity_us = esp_timer_get_time();
    xSemaphoreGive(s_mutex);
}

void display_ui_set_mood(lobster_mood_t mood)
{
    s_mood = mood;
    s_last_activity_us = esp_timer_get_time();
}

void display_ui_notify_message(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_msg_count++;
    s_last_activity_us = esp_timer_get_time();

    /* Activer le banner */
    s_banner_active = true;
    s_banner_start_us = esp_timer_get_time();
    strncpy(s_banner_text, s_message, sizeof(s_banner_text) - 1);
    s_banner_text[sizeof(s_banner_text) - 1] = '\0';

    /* Mood excited temporairement */
    s_mood = MOOD_EXCITED;

    /* Sortir du screensaver si besoin */
    if (s_state == DISPLAY_SCREENSAVER) {
        s_state = DISPLAY_IDLE;
    }
    xSemaphoreGive(s_mutex);
}

/* ---- Etch-a-sketch API ---- */
#ifdef MIMI_HAS_SERVOS

void display_ui_etch_set_cursor(int x, int y)
{
    if (x < 0) x = 0;
    if (x >= ETCH_W) x = ETCH_W - 1;
    if (y < 0) y = 0;
    if (y >= ETCH_H) y = ETCH_H - 1;
    s_etch_cursor_x = x;
    s_etch_cursor_y = y;
}

void display_ui_etch_set_drawing(bool drawing)
{
    s_etch_drawing = drawing;
}

void display_ui_etch_clear(void)
{
    if (s_etch_canvas) {
        memset(s_etch_canvas, 0, ETCH_W * ETCH_H);
    }
}

void display_ui_etch_next_color(void)
{
    s_etch_color_idx++;
    if (s_etch_color_idx > ETCH_NUM_COLORS) s_etch_color_idx = 1;
}
#endif /* MIMI_HAS_SERVOS */
