#include "display_hal.h"
#include "mimi_config.h"

#include "esp_log.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/gpio.h"

static const char *TAG = "disp_hal";

static esp_lcd_panel_handle_t s_panel = NULL;

esp_err_t display_hal_init(void)
{
    /* Backlight + power GPIO */
    gpio_config_t bl_cfg = {
        .pin_bit_mask = (1ULL << MIMI_DISP_PIN_BL) | (1ULL << MIMI_DISP_PIN_POWER),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&bl_cfg);
    gpio_set_level(MIMI_DISP_PIN_POWER, 1);
    gpio_set_level(MIMI_DISP_PIN_BL, 1);

    /* RD pin — pas utilise en ecriture, maintenir HIGH */
    gpio_config_t rd_cfg = {
        .pin_bit_mask = (1ULL << MIMI_DISP_PIN_RD),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&rd_cfg);
    gpio_set_level(MIMI_DISP_PIN_RD, 1);

    /* Bus I80 (Intel 8080 parallel 8-bit) */
    esp_lcd_i80_bus_handle_t bus = NULL;
    esp_lcd_i80_bus_config_t bus_cfg = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .dc_gpio_num = MIMI_DISP_PIN_DC,
        .wr_gpio_num = MIMI_DISP_PIN_WR,
        .data_gpio_nums = {
            MIMI_DISP_PIN_D0, MIMI_DISP_PIN_D1,
            MIMI_DISP_PIN_D2, MIMI_DISP_PIN_D3,
            MIMI_DISP_PIN_D4, MIMI_DISP_PIN_D5,
            MIMI_DISP_PIN_D6, MIMI_DISP_PIN_D7,
        },
        .bus_width = 8,
        .max_transfer_bytes = MIMI_DISP_WIDTH * MIMI_DISP_BUF_LINES * sizeof(uint16_t),
        .psram_trans_align = 64,
        .sram_trans_align = 4,
    };
    ESP_ERROR_CHECK(esp_lcd_new_i80_bus(&bus_cfg, &bus));

    /* Panel IO */
    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_panel_io_i80_config_t io_cfg = {
        .cs_gpio_num = MIMI_DISP_PIN_CS,
        .pclk_hz = 10 * 1000 * 1000,  /* 10 MHz */
        .trans_queue_depth = 10,
        .dc_levels = {
            .dc_idle_level = 0,
            .dc_cmd_level = 0,
            .dc_dummy_level = 0,
            .dc_data_level = 1,
        },
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i80(bus, &io_cfg, &io));

    /* Panel ST7789 */
    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = MIMI_DISP_PIN_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io, &panel_cfg, &s_panel));

    /* Init sequence */
    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel));

    /* T-Display S3 : inversion des couleurs + rotation portrait */
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(s_panel, true));
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(s_panel, true));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(s_panel, true, false));

    /* Allumer l'ecran */
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_panel, true));

    ESP_LOGI(TAG, "Display ST7789 %dx%d init OK", MIMI_DISP_WIDTH, MIMI_DISP_HEIGHT);
    return ESP_OK;
}

esp_err_t display_hal_flush(int x, int y, int w, int h, const uint16_t *data)
{
    if (!s_panel) return ESP_ERR_INVALID_STATE;
    return esp_lcd_panel_draw_bitmap(s_panel, x, y, x + w, y + h, data);
}

void display_hal_backlight(bool on)
{
    gpio_set_level(MIMI_DISP_PIN_BL, on ? 1 : 0);
}

void display_hal_sleep(void)
{
    if (s_panel) {
        esp_lcd_panel_disp_on_off(s_panel, false);
    }
    display_hal_backlight(false);
    ESP_LOGI(TAG, "Display sleep");
}

void display_hal_wake(void)
{
    display_hal_backlight(true);
    if (s_panel) {
        esp_lcd_panel_disp_on_off(s_panel, true);
    }
    ESP_LOGI(TAG, "Display wake");
}
