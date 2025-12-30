#ifndef TELEMETRY_SD_H
#define TELEMETRY_SD_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h> // <--- ADICIONADO PARA CORRIGIR O ERRO 'size_t'
#include "telemetry_gps.h"
#include "telemetry_mpu.h"

#ifdef __cplusplus
extern "C" {
#endif

bool sd_init(void);
void sd_start_new_session(gps_data_t gps);
void sd_stop_session(void);
uint16_t sd_get_current_session_id(void);

void sd_log_sample(gps_data_t gps, mpu_data_t mpu, race_mode_t mode, uint16_t lap);
void sd_save_lap_event(uint16_t lap, uint32_t ms, float avg_speed, gps_data_t gps, race_mode_t mode);

void sd_get_info(float *used_gb, float *total_gb);
int sd_get_available_sessions(uint16_t *session_list, int max_sessions);

// Função que lista os nomes reais dos arquivos
int sd_get_session_string_list(char *buffer, size_t max_len);

void sd_load_session_history(uint16_t session_id);
void sd_delete_all_sessions(void);

#ifdef __cplusplus
}
#endif

#endif