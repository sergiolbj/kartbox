#ifndef TELEMETRY_SD_H
#define TELEMETRY_SD_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "telemetry_gps.h"
#include "telemetry_mpu.h"
#include "driver/sdmmc_host.h" // <--- Importante para o tipo sdmmc_card_t

// Inicializa o cartão SD
bool sd_init(void);

// Inicia/Para sessões
void sd_start_new_session(gps_data_t gps);
void sd_stop_session(void);

// Gravação de dados
void sd_log_sample(gps_data_t gps, mpu_data_t mpu, race_mode_t mode, uint16_t lap);
void sd_save_lap_event(uint16_t lap, uint32_t ms, float avg_speed, gps_data_t gps, race_mode_t mode);

// Gerenciamento de arquivos
int sd_get_available_sessions(uint16_t *session_list, int max);
int sd_get_session_string_list(char *buffer, size_t max_len);
void sd_load_session_history(uint16_t idx);
void sd_delete_all_sessions(void);
void sd_get_info(float *used_gb, float *total_gb);
uint16_t sd_get_current_session_id(void);

// --- NOVO: Função para o USB pegar o controle do cartão ---
sdmmc_card_t* sd_get_card_handle(void);

#endif