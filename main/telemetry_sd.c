#include "telemetry_sd.h"
#include "config.h"
#include "ui_kartbox.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "sdmmc_cmd.h"
#include "esp_ldo_regulator.h" 
#include <dirent.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

// Variáveis de estado
static bool mounted = false;
static FILE *f_telemetry = NULL;
static uint16_t current_session_id = 1;
static char session_filename[128] = {0}; 

bool sd_init(void) {
    // Configuração do LDO (Regulador) - Ajuste conforme seu hardware
    esp_ldo_channel_handle_t ldo_h;
    esp_ldo_channel_config_t ldo_c = { .chan_id = 4, .voltage_mv = 3300 };
    esp_ldo_acquire_channel(&ldo_c, &ldo_h);

    gpio_reset_pin(SD_PWR_EN_PIN);
    gpio_set_direction(SD_PWR_EN_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(SD_PWR_EN_PIN, 0); 
    vTaskDelay(pdMS_TO_TICKS(500));

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.slot = SDMMC_HOST_SLOT_1;
    host.command_timeout_ms = 1000;

    sdmmc_slot_config_t slot_cfg = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_cfg.clk = SD_CLK_PIN; slot_cfg.cmd = SD_CMD_PIN; slot_cfg.d0 = SD_D0_PIN;
    slot_cfg.width = 1;

    esp_vfs_fat_sdmmc_mount_config_t mnt_cfg = { .format_if_mount_failed = false, .max_files = 5 };
    sdmmc_card_t *card;
    if (esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_cfg, &mnt_cfg, &card) != ESP_OK) return false;

    mounted = true;
    
    // Recupera último ID
    FILE *f = fopen("/sdcard/last_id.txt", "r");
    if (f) { fscanf(f, "%hu", &current_session_id); fclose(f); }
    
    return true;
}

void sd_start_new_session(gps_data_t gps) {
    if (f_telemetry) fclose(f_telemetry);
    current_session_id++;
    
    FILE *f_id = fopen("/sdcard/last_id.txt", "w");
    if (f_id) { fprintf(f_id, "%hu", current_session_id); fclose(f_id); }

    if (gps.valid) snprintf(session_filename, 128, "20%02d%02d%02d_%02d%02d", gps.year, gps.month, gps.day, gps.hour, gps.minute);
    else snprintf(session_filename, 128, "RUN_%03d", current_session_id);
    
    char path[256]; snprintf(path, 256, "/sdcard/data_%s.csv", session_filename);
    f_telemetry = fopen(path, "w");
    if (f_telemetry) fprintf(f_telemetry, "Timestamp_ms,Lat,Lon,Speed,Mode,Lap\n");
}

void sd_stop_session(void) { if (f_telemetry) { fclose(f_telemetry); f_telemetry = NULL; } }

void sd_save_lap_event(uint16_t lap, uint32_t ms, float avg_speed, gps_data_t gps, race_mode_t mode) {
    if (!mounted) return;
    char path[256]; snprintf(path, 256, "/sdcard/laps_%s.csv", session_filename);
    FILE *fl = fopen(path, "a+");
    if (fl) { 
        fprintf(fl, "%d,%lu.%03lu,%.1f\n", lap, ms/1000, ms%1000, avg_speed); 
        fclose(fl); 
    }
}

// Mantido para compatibilidade
int sd_get_available_sessions(uint16_t *session_list, int max) {
    if (!mounted) return 0;
    DIR *dir = opendir("/sdcard"); if (!dir) return 0;
    struct dirent *ent; int count = 0;
    while ((ent = readdir(dir)) && count < max) {
        if (strstr(ent->d_name, "laps_")) { session_list[count] = count + 1; count++; }
    }
    closedir(dir); return count;
}

// --- FUNÇÃO CORRIGIDA PARA O NOME DOS ARQUIVOS ---
int sd_get_session_string_list(char *buffer, size_t max_len) {
    if (!mounted || !buffer) return 0;
    buffer[0] = '\0';
    
    DIR *dir = opendir("/sdcard");
    if (!dir) return 0;
    
    struct dirent *ent;
    int count = 0;
    size_t current_len = 0;
    
    while ((ent = readdir(dir))) {
        if (strstr(ent->d_name, "laps_") && strstr(ent->d_name, ".csv")) {
            char clean_name[64];
            
            char *start = ent->d_name + 5; // Pula "laps_"
            char *end = strstr(ent->d_name, ".csv");
            
            if (start && end && end > start) {
                // Cálculo seguro do tamanho
                size_t len = end - start;
                if (len > sizeof(clean_name) - 1) len = sizeof(clean_name) - 1;
                
                // [CORREÇÃO] Usando memcpy ao invés de strncpy para evitar o erro do compilador
                memcpy(clean_name, start, len);
                clean_name[len] = '\0'; // Garante o fim da string manualmente
            } else {
                // [CORREÇÃO] Usando snprintf para segurança
                snprintf(clean_name, sizeof(clean_name), "%s", ent->d_name);
            }
            
            size_t added = snprintf(buffer + current_len, max_len - current_len, "%s\n", clean_name);
            if (added > 0) {
                current_len += added;
                if (current_len >= max_len - 1) break;
            }
            count++;
        }
    }
    closedir(dir);
    
    if (current_len > 0 && buffer[current_len - 1] == '\n') {
        buffer[current_len - 1] = '\0';
    }
    return count;
}

void sd_load_session_history(uint16_t idx) {
    if (!mounted) return;
    DIR *dir = opendir("/sdcard"); if (!dir) return;
    struct dirent *ent; int count = 0; char target[256] = "";
    
    while ((ent = readdir(dir))) {
        if (strstr(ent->d_name, "laps_") && strstr(ent->d_name, ".csv")) {
            if (count == idx) { snprintf(target, 256, "/sdcard/%s", ent->d_name); break; }
            count++;
        }
    }
    closedir(dir);
    
    if (strlen(target) == 0) return;
    
    FILE *f = fopen(target, "r"); if (!f) return;
    char line[128];
    while (fgets(line, 128, f)) {
        int lap; unsigned long s, ms; float avg;
        if (sscanf(line, "%d,%lu.%lu,%f", &lap, &s, &ms, &avg) >= 4) {
            ui_add_lap_to_list(lap, (s * 1000) + ms);
            ui_add_point_to_chart(avg);
        }
    }
    fclose(f);
}

void sd_get_info(float *used_gb, float *total_gb) {
    if (!mounted) { *used_gb = 0; *total_gb = 0; return; }
    FATFS *fs; DWORD fre_clust, tot_sect;
    if (f_getfree("0:", &fre_clust, &fs) == FR_OK) {
        tot_sect = (fs->n_fatent - 2) * fs->csize;
        *total_gb = (float)tot_sect * 512 / (1024.0f * 1024.0f * 1024.0f);
        *used_gb = *total_gb - ((float)fre_clust * fs->csize * 512 / (1024.0f * 1024.0f * 1024.0f));
    }
}

void sd_log_sample(gps_data_t gps, mpu_data_t mpu, race_mode_t mode, uint16_t lap) {
    if (f_telemetry) {
        fprintf(f_telemetry, "%lu,%.6f,%.6f,%.1f,%d,%d\n", gps.timestamp_ms, gps.lat, gps.lon, gps.speed_kmh, (int)mode, lap);
    }
}

void sd_delete_all_sessions(void) {
    DIR *dir = opendir("/sdcard"); if (!dir) return;
    struct dirent *ent;
    while ((ent = readdir(dir))) {
        if (strstr(ent->d_name, ".csv")) {
            char p[256]; snprintf(p, 256, "/sdcard/%s", ent->d_name); unlink(p);
        }
    }
    closedir(dir);
}

uint16_t sd_get_current_session_id(void) { 
    return current_session_id; 
}