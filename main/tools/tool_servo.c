#include "tool_servo.h"
#include "hardware/servo_driver.h"
#include "hardware/body_animator.h"

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "cJSON.h"

static const char *TAG = "tool_servo";

esp_err_t tool_move_head_execute(const char *input_json, char *output, size_t output_size)
{
    cJSON *root = cJSON_Parse(input_json);
    if (!root) {
        snprintf(output, output_size, "Error: invalid JSON");
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *h = cJSON_GetObjectItem(root, "horizontal");
    cJSON *v = cJSON_GetObjectItem(root, "vertical");

    if (h && cJSON_IsNumber(h)) {
        servo_set_angle(SERVO_HEAD_H, (uint8_t)h->valueint);
    }
    if (v && cJSON_IsNumber(v)) {
        servo_set_angle(SERVO_HEAD_V, (uint8_t)v->valueint);
    }

    snprintf(output, output_size, "Head moved to H=%d V=%d",
             servo_get_angle(SERVO_HEAD_H), servo_get_angle(SERVO_HEAD_V));

    cJSON_Delete(root);
    ESP_LOGI(TAG, "move_head: %s", output);
    return ESP_OK;
}

esp_err_t tool_move_claw_execute(const char *input_json, char *output, size_t output_size)
{
    cJSON *root = cJSON_Parse(input_json);
    if (!root) {
        snprintf(output, output_size, "Error: invalid JSON");
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *side  = cJSON_GetObjectItem(root, "side");
    cJSON *angle = cJSON_GetObjectItem(root, "angle");

    if (!angle || !cJSON_IsNumber(angle)) {
        snprintf(output, output_size, "Error: 'angle' required");
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t a = (uint8_t)angle->valueint;
    const char *s = (side && cJSON_IsString(side)) ? side->valuestring : "both";

    if (strcmp(s, "left") == 0 || strcmp(s, "both") == 0) {
        servo_set_angle(SERVO_CLAW_L, a);
    }
    if (strcmp(s, "right") == 0 || strcmp(s, "both") == 0) {
        servo_set_angle(SERVO_CLAW_R, a);
    }

    snprintf(output, output_size, "Claw %s set to %d° (L=%d R=%d)",
             s, a, servo_get_angle(SERVO_CLAW_L), servo_get_angle(SERVO_CLAW_R));

    cJSON_Delete(root);
    ESP_LOGI(TAG, "move_claw: %s", output);
    return ESP_OK;
}

esp_err_t tool_read_distance_execute(const char *input_json, char *output, size_t output_size)
{
    (void)input_json;
    int dist = body_animator_get_distance();

    if (dist < 0) {
        snprintf(output, output_size, "No object detected (out of range or sensor error)");
    } else {
        snprintf(output, output_size, "Distance: %d cm", dist);
    }

    ESP_LOGI(TAG, "read_distance: %s", output);
    return ESP_OK;
}

esp_err_t tool_animate_execute(const char *input_json, char *output, size_t output_size)
{
    cJSON *root = cJSON_Parse(input_json);
    if (!root) {
        snprintf(output, output_size, "Error: invalid JSON");
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *anim = cJSON_GetObjectItem(root, "animation");
    if (!anim || !cJSON_IsString(anim)) {
        snprintf(output, output_size, "Error: 'animation' required (wave, nod_yes, nod_no, celebrate, think, sleep)");
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }

    body_animator_play(anim->valuestring);

    snprintf(output, output_size, "Animation '%s' played", anim->valuestring);
    cJSON_Delete(root);
    ESP_LOGI(TAG, "animate: %s", output);
    return ESP_OK;
}
