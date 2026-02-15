#include "captive_portal.h"
#include "mimi_config.h"
#include "wifi/wifi_manager.h"
#include "telegram/telegram_bot.h"
#include "llm/llm_proxy.h"
#include "proxy/http_proxy.h"
#include "tools/tool_web_search.h"

#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_system.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"

static const char *TAG = "portal";

static httpd_handle_t s_httpd = NULL;
static TaskHandle_t s_dns_task = NULL;
static bool s_active = false;

/* ================================================================
 * PAGE HTML EMBEDDED — Design sombre MimiClaw
 * ================================================================ */

static const char PORTAL_HTML[] =
"<!DOCTYPE html><html lang='fr'><head>"
"<meta charset='utf-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>MimiClaw Setup</title>"
"<style>"
"*{margin:0;padding:0;box-sizing:border-box}"
"body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;"
"background:linear-gradient(135deg,#0f0c29,#1a1a2e,#16213e);color:#e0e0e0;"
"min-height:100vh;padding:20px}"
".container{max-width:440px;margin:0 auto}"
".header{text-align:center;padding:30px 0 20px}"
".logo{font-size:48px;margin-bottom:8px;animation:float 3s ease-in-out infinite}"
"@keyframes float{0%,100%{transform:translateY(0)}50%{transform:translateY(-8px)}}"
"h1{font-size:22px;background:linear-gradient(90deg,#a855f7,#06b6d4);-webkit-background-clip:text;"
"-webkit-text-fill-color:transparent;margin-bottom:4px}"
".subtitle{font-size:12px;color:#666;letter-spacing:2px}"
".card{background:rgba(255,255,255,0.05);border:1px solid rgba(255,255,255,0.08);"
"border-radius:12px;padding:20px;margin:16px 0;backdrop-filter:blur(10px);"
"animation:fadeIn 0.5s ease}"
"@keyframes fadeIn{from{opacity:0;transform:translateY(10px)}to{opacity:1;transform:translateY(0)}}"
".card-title{font-size:13px;font-weight:700;color:#a855f7;text-transform:uppercase;"
"letter-spacing:1.5px;margin-bottom:14px;display:flex;align-items:center;gap:8px}"
".field{margin-bottom:14px}"
"label{display:block;font-size:12px;color:#888;margin-bottom:4px}"
".input-wrap{position:relative}"
"input[type=text],input[type=password],input[type=number]{width:100%;padding:10px 14px;"
"background:rgba(0,0,0,0.3);border:1px solid rgba(255,255,255,0.1);border-radius:8px;"
"color:#fff;font-size:14px;outline:none;transition:border 0.2s}"
"input:focus{border-color:#a855f7}"
"input::placeholder{color:#444}"
".toggle-pw{position:absolute;right:10px;top:50%;transform:translateY(-50%);"
"background:none;border:none;color:#666;cursor:pointer;font-size:16px}"
".btn{width:100%;padding:14px;background:linear-gradient(135deg,#a855f7,#7c3aed);"
"border:none;border-radius:10px;color:#fff;font-size:15px;font-weight:600;"
"cursor:pointer;transition:all 0.3s;letter-spacing:0.5px;margin-top:10px}"
".btn:hover{transform:translateY(-1px);box-shadow:0 4px 20px rgba(168,85,247,0.4)}"
".btn:active{transform:translateY(0)}"
".btn:disabled{opacity:0.5;cursor:not-allowed;transform:none}"
".btn-reboot{background:linear-gradient(135deg,#06b6d4,#0891b2)}"
".status{text-align:center;padding:12px;border-radius:8px;margin-top:12px;font-size:13px;display:none}"
".status.ok{display:block;background:rgba(34,197,94,0.15);color:#22c55e;border:1px solid rgba(34,197,94,0.2)}"
".status.err{display:block;background:rgba(239,68,68,0.15);color:#ef4444;border:1px solid rgba(239,68,68,0.2)}"
".footer{text-align:center;padding:24px 0;color:#444;font-size:11px;letter-spacing:1px}"
".footer a{color:#666;text-decoration:none}"
".spinner{display:inline-block;width:16px;height:16px;border:2px solid rgba(255,255,255,0.3);"
"border-top-color:#fff;border-radius:50%;animation:spin 0.6s linear infinite;vertical-align:middle;margin-right:6px}"
"@keyframes spin{to{transform:rotate(360deg)}}"
".hint{font-size:11px;color:#555;margin-top:3px}"
"</style></head><body>"
"<div class='container'>"
"<div class='header'>"
"<div class='logo'>&#128049;</div>"
"<h1>MimiClaw Setup</h1>"
"<div class='subtitle'>CONFIGURATION PORTAL</div>"
"</div>"

/* WiFi Card */
"<div class='card' style='animation-delay:0.1s'>"
"<div class='card-title'>&#128246; WiFi</div>"
"<div class='field'><label>SSID</label>"
"<input type='text' id='wifi_ssid' placeholder='Nom du reseau WiFi'></div>"
"<div class='field'><label>Mot de passe</label>"
"<div class='input-wrap'><input type='password' id='wifi_pass' placeholder='Mot de passe WiFi'>"
"<button class='toggle-pw' onclick=\"togglePw('wifi_pass')\">&#128065;</button></div></div>"
"</div>"

/* Telegram Card */
"<div class='card' style='animation-delay:0.2s'>"
"<div class='card-title'>&#128172; Telegram</div>"
"<div class='field'><label>Bot Token</label>"
"<div class='input-wrap'><input type='password' id='tg_token' placeholder='123456:ABC-DEF...'>"
"<button class='toggle-pw' onclick=\"togglePw('tg_token')\">&#128065;</button></div>"
"<div class='hint'>Obtenu via @BotFather sur Telegram</div></div>"
"</div>"

/* Claude API Card */
"<div class='card' style='animation-delay:0.3s'>"
"<div class='card-title'>&#129302; Claude API</div>"
"<div class='field'><label>API Key</label>"
"<div class='input-wrap'><input type='password' id='api_key' placeholder='sk-ant-api03-...'>"
"<button class='toggle-pw' onclick=\"togglePw('api_key')\">&#128065;</button></div></div>"
"<div class='field'><label>Model</label>"
"<input type='text' id='model' placeholder='claude-sonnet-4-5 (defaut si vide)'></div>"
"</div>"

/* Proxy Card */
"<div class='card' style='animation-delay:0.4s'>"
"<div class='card-title'>&#128274; Proxy (optionnel)</div>"
"<div class='field'><label>Host</label>"
"<input type='text' id='proxy_host' placeholder='ex: 192.168.1.100'></div>"
"<div class='field'><label>Port</label>"
"<input type='number' id='proxy_port' placeholder='ex: 7897'></div>"
"</div>"

/* Search Card */
"<div class='card' style='animation-delay:0.5s'>"
"<div class='card-title'>&#128270; Brave Search (optionnel)</div>"
"<div class='field'><label>API Key</label>"
"<div class='input-wrap'><input type='password' id='search_key' placeholder='BSA...'>"
"<button class='toggle-pw' onclick=\"togglePw('search_key')\">&#128065;</button></div></div>"
"</div>"

/* Buttons */
"<button class='btn' id='saveBtn' onclick='saveConfig()'>Sauvegarder & Redemarrer</button>"
"<div id='status' class='status'></div>"

"<div class='footer'>by <a href='#'>Silexperience</a></div>"
"</div>"

"<script>"
"function togglePw(id){"
"var e=document.getElementById(id);"
"e.type=e.type==='password'?'text':'password'}"

"function showStatus(msg,ok){"
"var s=document.getElementById('status');"
"s.textContent=msg;s.className='status '+(ok?'ok':'err')}"

"async function loadConfig(){"
"try{var r=await fetch('/api/config');var d=await r.json();"
"if(d.wifi_ssid)document.getElementById('wifi_ssid').value=d.wifi_ssid;"
"if(d.model)document.getElementById('model').value=d.model;"
"if(d.proxy_host)document.getElementById('proxy_host').value=d.proxy_host;"
"if(d.proxy_port)document.getElementById('proxy_port').value=d.proxy_port;"
"}catch(e){}}"

"async function saveConfig(){"
"var btn=document.getElementById('saveBtn');"
"btn.disabled=true;btn.innerHTML='<span class=\"spinner\"></span>Sauvegarde...';"
"var cfg={"
"wifi_ssid:document.getElementById('wifi_ssid').value,"
"wifi_pass:document.getElementById('wifi_pass').value,"
"tg_token:document.getElementById('tg_token').value,"
"api_key:document.getElementById('api_key').value,"
"model:document.getElementById('model').value,"
"proxy_host:document.getElementById('proxy_host').value,"
"proxy_port:document.getElementById('proxy_port').value,"
"search_key:document.getElementById('search_key').value};"
"try{var r=await fetch('/api/save',{method:'POST',"
"headers:{'Content-Type':'application/json'},body:JSON.stringify(cfg)});"
"var d=await r.json();"
"if(d.ok){showStatus('Configuration sauvegardee ! Redemarrage...',true);"
"setTimeout(function(){fetch('/api/reboot',{method:'POST'})},1500)}"
"else showStatus('Erreur: '+(d.error||'inconnue'),false)}"
"catch(e){showStatus('Erreur reseau',false)}"
"finally{btn.disabled=false;btn.innerHTML='Sauvegarder & Redemarrer'}}"

"loadConfig();"
"</script></body></html>";

/* ================================================================
 * HELPER: lire config NVS
 * ================================================================ */

static char *build_config_json(void)
{
    cJSON *root = cJSON_CreateObject();
    char buf[128];
    nvs_handle_t nvs;

    /* WiFi SSID (non masque) */
    buf[0] = '\0';
    if (nvs_open(MIMI_NVS_WIFI, NVS_READONLY, &nvs) == ESP_OK) {
        size_t len = sizeof(buf);
        if (nvs_get_str(nvs, MIMI_NVS_KEY_SSID, buf, &len) != ESP_OK) buf[0] = '\0';
        nvs_close(nvs);
    }
    if (buf[0] == '\0' && MIMI_SECRET_WIFI_SSID[0]) {
        strncpy(buf, MIMI_SECRET_WIFI_SSID, sizeof(buf) - 1);
    }
    cJSON_AddStringToObject(root, "wifi_ssid", buf);

    /* Model (non masque) */
    buf[0] = '\0';
    if (nvs_open(MIMI_NVS_LLM, NVS_READONLY, &nvs) == ESP_OK) {
        size_t len = sizeof(buf);
        if (nvs_get_str(nvs, MIMI_NVS_KEY_MODEL, buf, &len) != ESP_OK) buf[0] = '\0';
        nvs_close(nvs);
    }
    if (buf[0] == '\0' && MIMI_SECRET_MODEL[0]) {
        strncpy(buf, MIMI_SECRET_MODEL, sizeof(buf) - 1);
    }
    cJSON_AddStringToObject(root, "model", buf);

    /* Proxy host (non masque) */
    buf[0] = '\0';
    if (nvs_open(MIMI_NVS_PROXY, NVS_READONLY, &nvs) == ESP_OK) {
        size_t len = sizeof(buf);
        if (nvs_get_str(nvs, MIMI_NVS_KEY_PROXY_HOST, buf, &len) != ESP_OK) buf[0] = '\0';
        nvs_close(nvs);
    }
    if (buf[0] == '\0' && MIMI_SECRET_PROXY_HOST[0]) {
        strncpy(buf, MIMI_SECRET_PROXY_HOST, sizeof(buf) - 1);
    }
    cJSON_AddStringToObject(root, "proxy_host", buf);

    /* Proxy port (non masque) */
    buf[0] = '\0';
    if (nvs_open(MIMI_NVS_PROXY, NVS_READONLY, &nvs) == ESP_OK) {
        size_t len = sizeof(buf);
        if (nvs_get_str(nvs, MIMI_NVS_KEY_PROXY_PORT, buf, &len) != ESP_OK) buf[0] = '\0';
        nvs_close(nvs);
    }
    if (buf[0] == '\0' && MIMI_SECRET_PROXY_PORT[0]) {
        strncpy(buf, MIMI_SECRET_PROXY_PORT, sizeof(buf) - 1);
    }
    cJSON_AddStringToObject(root, "proxy_port", buf);

    /* Cles masquees: wifi_pass, tg_token, api_key, search_key */
    /* On ne les retourne pas du tout — le frontend laisse les champs vides */

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}

/* ================================================================
 * HTTP HANDLERS
 * ================================================================ */

static esp_err_t handler_root(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, PORTAL_HTML, sizeof(PORTAL_HTML) - 1);
}

static esp_err_t handler_get_config(httpd_req_t *req)
{
    char *json = build_config_json();
    if (!json) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "JSON error");
        return ESP_FAIL;
    }
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, strlen(json));
    free(json);
    return ESP_OK;
}

static esp_err_t handler_save(httpd_req_t *req)
{
    /* Lire le body */
    int total_len = req->content_len;
    if (total_len <= 0 || total_len > 2048) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Body trop grand ou vide");
        return ESP_FAIL;
    }

    char *buf = calloc(1, total_len + 1);
    if (!buf) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memoire insuffisante");
        return ESP_FAIL;
    }

    int received = 0;
    while (received < total_len) {
        int ret = httpd_req_recv(req, buf + received, total_len - received);
        if (ret <= 0) { free(buf); return ESP_FAIL; }
        received += ret;
    }

    cJSON *root = cJSON_Parse(buf);
    free(buf);

    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "JSON invalide");
        return ESP_FAIL;
    }

    int saved = 0;

    /* WiFi */
    cJSON *ssid = cJSON_GetObjectItem(root, "wifi_ssid");
    cJSON *pass = cJSON_GetObjectItem(root, "wifi_pass");
    if (ssid && cJSON_IsString(ssid) && ssid->valuestring[0]) {
        const char *pw = (pass && cJSON_IsString(pass)) ? pass->valuestring : "";
        wifi_manager_set_credentials(ssid->valuestring, pw);
        saved++;
        ESP_LOGI(TAG, "WiFi credentials saved: %s", ssid->valuestring);
    }

    /* Telegram token */
    cJSON *tg = cJSON_GetObjectItem(root, "tg_token");
    if (tg && cJSON_IsString(tg) && tg->valuestring[0]) {
        telegram_set_token(tg->valuestring);
        saved++;
        ESP_LOGI(TAG, "Telegram token saved");
    }

    /* API key */
    cJSON *api = cJSON_GetObjectItem(root, "api_key");
    if (api && cJSON_IsString(api) && api->valuestring[0]) {
        llm_set_api_key(api->valuestring);
        saved++;
        ESP_LOGI(TAG, "API key saved");
    }

    /* Model */
    cJSON *model = cJSON_GetObjectItem(root, "model");
    if (model && cJSON_IsString(model) && model->valuestring[0]) {
        llm_set_model(model->valuestring);
        saved++;
        ESP_LOGI(TAG, "Model saved: %s", model->valuestring);
    }

    /* Proxy */
    cJSON *ph = cJSON_GetObjectItem(root, "proxy_host");
    cJSON *pp = cJSON_GetObjectItem(root, "proxy_port");
    if (ph && cJSON_IsString(ph) && ph->valuestring[0]
        && pp && cJSON_IsString(pp) && pp->valuestring[0]) {
        uint16_t port = (uint16_t)atoi(pp->valuestring);
        if (port > 0) {
            http_proxy_set(ph->valuestring, port);
            saved++;
            ESP_LOGI(TAG, "Proxy saved: %s:%d", ph->valuestring, port);
        }
    } else if ((!ph || !ph->valuestring[0]) && (!pp || !pp->valuestring[0])) {
        http_proxy_clear();
    }

    /* Search key */
    cJSON *sk = cJSON_GetObjectItem(root, "search_key");
    if (sk && cJSON_IsString(sk) && sk->valuestring[0]) {
        tool_web_search_set_key(sk->valuestring);
        saved++;
        ESP_LOGI(TAG, "Search key saved");
    }

    cJSON_Delete(root);

    /* Repondre */
    char resp[64];
    snprintf(resp, sizeof(resp), "{\"ok\":true,\"saved\":%d}", saved);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

static esp_err_t handler_reboot(httpd_req_t *req)
{
    const char *resp = "{\"ok\":true,\"rebooting\":true}";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));

    ESP_LOGI(TAG, "Reboot demande via portail");
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
    return ESP_OK; /* unreachable */
}

/* Catchall: redirige vers / (comportement captive portal) */
static esp_err_t handler_catchall(httpd_req_t *req)
{
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

/* ================================================================
 * DNS CAPTIF — repond a toute requete avec 192.168.4.1
 * ================================================================ */

static void dns_task(void *arg)
{
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        ESP_LOGE(TAG, "DNS socket failed");
        vTaskDelete(NULL);
        return;
    }

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(53),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(TAG, "DNS bind failed");
        close(sock);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "DNS captif demarre sur port 53");

    uint8_t buf[512];
    struct sockaddr_in client_addr;
    socklen_t client_len;

    while (1) {
        client_len = sizeof(client_addr);
        int len = recvfrom(sock, buf, sizeof(buf), 0,
                           (struct sockaddr *)&client_addr, &client_len);
        if (len < 12) continue; /* Paquet DNS trop court */

        /* Construire reponse DNS minimale:
         * - Copier le header + question du paquet original
         * - Ajouter une reponse A pointant vers 192.168.4.1
         */
        uint8_t resp[512];
        if (len > (int)(sizeof(resp) - 16)) continue;

        memcpy(resp, buf, len);

        /* Modifier header: QR=1 (reponse), AA=1, ANCOUNT=1 */
        resp[2] = 0x84; /* QR=1, AA=1 */
        resp[3] = 0x00; /* RCODE=0 (no error) */
        resp[6] = 0x00; resp[7] = 0x01; /* ANCOUNT = 1 */

        /* Ajouter answer apres la question:
         * Name pointer (0xC00C → pointe vers le nom dans la question)
         * Type A (0x0001), Class IN (0x0001), TTL 60s, RDLENGTH 4, IP
         */
        int pos = len;
        resp[pos++] = 0xC0; resp[pos++] = 0x0C; /* Name pointer */
        resp[pos++] = 0x00; resp[pos++] = 0x01; /* Type A */
        resp[pos++] = 0x00; resp[pos++] = 0x01; /* Class IN */
        resp[pos++] = 0x00; resp[pos++] = 0x00;
        resp[pos++] = 0x00; resp[pos++] = 0x3C; /* TTL = 60s */
        resp[pos++] = 0x00; resp[pos++] = 0x04; /* RDLENGTH = 4 */
        resp[pos++] = 192; resp[pos++] = 168;
        resp[pos++] = 4;   resp[pos++] = 1;     /* 192.168.4.1 */

        sendto(sock, resp, pos, 0,
               (struct sockaddr *)&client_addr, client_len);
    }

    close(sock);
    vTaskDelete(NULL);
}

/* ================================================================
 * START / STOP
 * ================================================================ */

esp_err_t captive_portal_start(void)
{
    if (s_active) {
        ESP_LOGW(TAG, "Portail deja actif");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Demarrage du portail captif...");

    /* 1. Demarrer le SoftAP */
    esp_err_t err = wifi_manager_start_ap(MIMI_PORTAL_AP_SSID, MIMI_PORTAL_AP_PASS);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Echec demarrage AP: %s", esp_err_to_name(err));
        return err;
    }

    /* 2. Demarrer le DNS captif */
    xTaskCreate(dns_task, "dns_captive", MIMI_PORTAL_DNS_STACK, NULL, 5, &s_dns_task);

    /* 3. Demarrer le serveur HTTP */
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = MIMI_PORTAL_HTTP_PORT;
    config.ctrl_port = MIMI_PORTAL_HTTP_PORT + 1;
    config.max_open_sockets = 4;
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.lru_purge_enable = true;

    err = httpd_start(&s_httpd, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Echec serveur HTTP: %s", esp_err_to_name(err));
        return err;
    }

    /* Routes (ordre important: specifiques avant catchall) */
    httpd_uri_t uri_root = {
        .uri = "/", .method = HTTP_GET, .handler = handler_root
    };
    httpd_uri_t uri_config = {
        .uri = "/api/config", .method = HTTP_GET, .handler = handler_get_config
    };
    httpd_uri_t uri_save = {
        .uri = "/api/save", .method = HTTP_POST, .handler = handler_save
    };
    httpd_uri_t uri_reboot = {
        .uri = "/api/reboot", .method = HTTP_POST, .handler = handler_reboot
    };
    httpd_uri_t uri_catchall = {
        .uri = "/*", .method = HTTP_GET, .handler = handler_catchall
    };

    httpd_register_uri_handler(s_httpd, &uri_root);
    httpd_register_uri_handler(s_httpd, &uri_config);
    httpd_register_uri_handler(s_httpd, &uri_save);
    httpd_register_uri_handler(s_httpd, &uri_reboot);
    httpd_register_uri_handler(s_httpd, &uri_catchall);

    s_active = true;
    ESP_LOGI(TAG, "Portail captif actif sur http://192.168.4.1");
    ESP_LOGI(TAG, "WiFi: %s / Mot de passe: %s", MIMI_PORTAL_AP_SSID, MIMI_PORTAL_AP_PASS);
    return ESP_OK;
}

esp_err_t captive_portal_stop(void)
{
    if (!s_active) return ESP_OK;

    ESP_LOGI(TAG, "Arret du portail captif...");

    if (s_httpd) {
        httpd_stop(s_httpd);
        s_httpd = NULL;
    }

    if (s_dns_task) {
        vTaskDelete(s_dns_task);
        s_dns_task = NULL;
    }

    wifi_manager_stop_ap();
    s_active = false;

    ESP_LOGI(TAG, "Portail captif arrete");
    return ESP_OK;
}

bool captive_portal_is_active(void)
{
    return s_active;
}
