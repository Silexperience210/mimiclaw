#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Info sur une mise a jour disponible */
typedef struct {
    bool available;       /* true si une version plus recente existe */
    char version[16];     /* ex: "1.3.2" */
    char url[256];        /* URL de telechargement du .bin */
    char sha256_hash[65]; /* Hash SHA256 du firmware (si disponible) */
} ota_update_info_t;

/**
 * Retourne la version actuelle du firmware.
 */
const char *ota_get_version(void);

/**
 * Retourne le suffixe de variante pour les assets GitHub.
 * "full" si MIMI_HAS_SERVOS, "display" si MIMI_HAS_DISPLAY, "base" sinon.
 */
const char *ota_get_variant(void);

/**
 * Verifie si une mise a jour est disponible sur GitHub Releases.
 * Interroge l'API GitHub, compare le tag avec la version actuelle.
 * @param info  Structure remplie avec les infos de la MAJ
 * @return ESP_OK si la verification a reussi (meme si pas de MAJ)
 */
esp_err_t ota_check_update(ota_update_info_t *info);

/**
 * Telecharge et installe une MAJ depuis une URL.
 * Log la progression. Reboot automatiquement en cas de succes.
 * @param url  URL HTTPS du fichier .bin
 * @return ESP_OK si succes (device reboote), erreur sinon
 */
esp_err_t ota_update_from_url(const char *url);

/**
 * Verifie si un rollback est possible (version precedente disponible).
 */
bool ota_can_rollback(void);

/**
 * Effectue un rollback vers la version precedente.
 * Reboot automatiquement.
 */
esp_err_t ota_rollback(void);

/**
 * Marque le firmware actuel comme valide (appeler apres un boot reussi).
 * Annule le rollback automatique.
 */
void ota_mark_valid(void);
