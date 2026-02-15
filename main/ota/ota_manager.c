#include "ota_manager.h"
#include "mimi_config.h"
#include "proxy/http_proxy.h"

#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_https_ota.h"
#include "esp_heap_caps.h"
#include "cJSON.h"

static const char *TAG = "ota";

/* --- Version --- */

const char *ota_get_version(void)
{
    return MIMI_FW_VERSION;
}

const char *ota_get_variant(void)
{
#ifdef MIMI_HAS_SERVOS
    return "full";
#elif defined(MIMI_HAS_DISPLAY)
    return "display";
#else
    return "base";
#endif
}

/* --- Comparaison semver simple --- */

static bool parse_version(const char *str, int *major, int *minor, int *patch)
{
    /* Accepte "v1.3.2" ou "1.3.2" */
    if (str[0] == 'v' || str[0] == 'V') str++;
    return sscanf(str, "%d.%d.%d", major, minor, patch) == 3;
}

static bool is_newer(const char *remote_tag)
{
    int rm, rn, rp, lm, ln, lp;
    if (!parse_version(remote_tag, &rm, &rn, &rp)) return false;
    if (!parse_version(MIMI_FW_VERSION, &lm, &ln, &lp)) return false;

    if (rm != lm) return rm > lm;
    if (rn != ln) return rn > ln;
    return rp > lp;
}

/* --- HTTP helper pour GET GitHub API --- */

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} resp_buf_t;

static esp_err_t http_event_cb(esp_http_client_event_t *evt)
{
    resp_buf_t *rb = (resp_buf_t *)evt->user_data;
    if (evt->event_id == HTTP_EVENT_ON_DATA && rb) {
        if (rb->len + evt->data_len < rb->cap) {
            memcpy(rb->data + rb->len, evt->data, evt->data_len);
            rb->len += evt->data_len;
            rb->data[rb->len] = '\0';
        }
    }
    return ESP_OK;
}

static char *github_api_get(const char *url)
{
    resp_buf_t rb = {
        .data = heap_caps_calloc(1, 4096, MALLOC_CAP_SPIRAM),
        .len = 0,
        .cap = 4096,
    };
    if (!rb.data) return NULL;

    esp_http_client_config_t config = {
        .url = url,
        .event_handler = http_event_cb,
        .user_data = &rb,
        .timeout_ms = 15000,
        .buffer_size = 2048,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) { free(rb.data); return NULL; }

    /* GitHub API requiert User-Agent */
    esp_http_client_set_header(client, "User-Agent", "LilyClaw-OTA");
    esp_http_client_set_header(client, "Accept", "application/vnd.github.v3+json");

    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK || status != 200) {
        ESP_LOGE(TAG, "GitHub API failed: err=%s status=%d", esp_err_to_name(err), status);
        free(rb.data);
        return NULL;
    }

    return rb.data;
}

/* --- Check update --- */

esp_err_t ota_check_update(ota_update_info_t *info)
{
    memset(info, 0, sizeof(*info));

    ESP_LOGI(TAG, "Checking for updates (current: v%s, variant: %s)",
             MIMI_FW_VERSION, ota_get_variant());

    char *json = github_api_get(MIMI_GITHUB_RELEASES_URL);
    if (!json) {
        ESP_LOGE(TAG, "Failed to fetch release info");
        return ESP_FAIL;
    }

    cJSON *root = cJSON_Parse(json);
    free(json);
    if (!root) {
        ESP_LOGE(TAG, "Failed to parse release JSON");
        return ESP_FAIL;
    }

    /* tag_name → "v1.3.2" */
    cJSON *tag = cJSON_GetObjectItem(root, "tag_name");
    if (!tag || !cJSON_IsString(tag)) {
        ESP_LOGE(TAG, "No tag_name in release");
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Latest release: %s", tag->valuestring);

    if (!is_newer(tag->valuestring)) {
        ESP_LOGI(TAG, "Already on latest version");
        cJSON_Delete(root);
        return ESP_OK; /* info->available reste false */
    }

    /* Chercher l'asset correspondant a notre variante */
    const char *variant = ota_get_variant();
    char suffix[32];
    snprintf(suffix, sizeof(suffix), "-%s-app-", variant);

    cJSON *assets = cJSON_GetObjectItem(root, "assets");
    if (assets && cJSON_IsArray(assets)) {
        cJSON *asset;
        cJSON_ArrayForEach(asset, assets) {
            cJSON *name = cJSON_GetObjectItem(asset, "name");
            if (!name || !cJSON_IsString(name)) continue;

            if (strstr(name->valuestring, suffix)) {
                cJSON *url = cJSON_GetObjectItem(asset, "browser_download_url");
                if (url && cJSON_IsString(url)) {
                    info->available = true;
                    /* Copier version sans le 'v' */
                    const char *ver = tag->valuestring;
                    if (ver[0] == 'v') ver++;
                    strncpy(info->version, ver, sizeof(info->version) - 1);
                    strncpy(info->url, url->valuestring, sizeof(info->url) - 1);

                    ESP_LOGI(TAG, "Update available: v%s → %s", info->version, name->valuestring);
                    break;
                }
            }
        }
    }

    if (!info->available) {
        ESP_LOGW(TAG, "Release %s found but no matching asset for variant '%s'",
                 tag->valuestring, variant);
    }

    cJSON_Delete(root);
    return ESP_OK;
}

/* --- OTA download + install --- */

esp_err_t ota_update_from_url(const char *url)
{
    ESP_LOGI(TAG, "Starting OTA from: %s", url);

    esp_http_client_config_t http_config = {
        .url = url,
        .timeout_ms = 120000,
        .buffer_size = 4096,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_https_ota_config_t ota_config = {
        .http_config = &http_config,
    };

    esp_https_ota_handle_t handle = NULL;
    esp_err_t ret = esp_https_ota_begin(&ota_config, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "OTA begin failed: %s", esp_err_to_name(ret));
        return ret;
    }

    int total = esp_https_ota_get_image_size(handle);
    ESP_LOGI(TAG, "Firmware size: %d bytes", total);

    int last_pct = -1;
    while (1) {
        ret = esp_https_ota_perform(handle);
        if (ret != ESP_ERR_HTTPS_OTA_IN_PROGRESS) break;

        int read = esp_https_ota_get_image_len_read(handle);
        int pct = (total > 0) ? (read * 100 / total) : 0;
        if (pct != last_pct && pct % 10 == 0) {
            ESP_LOGI(TAG, "OTA progress: %d%% (%d/%d)", pct, read, total);
            last_pct = pct;
        }
    }

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "OTA perform failed: %s", esp_err_to_name(ret));
        esp_https_ota_abort(handle);
        return ret;
    }

    ret = esp_https_ota_finish(handle);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "OTA successful! Restarting...");
        esp_restart();
    } else {
        ESP_LOGE(TAG, "OTA finish failed: %s", esp_err_to_name(ret));
    }

    return ret;
}
