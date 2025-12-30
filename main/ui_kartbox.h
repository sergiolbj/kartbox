#ifndef UI_KARTBOX_H
#define UI_KARTBOX_H

#include "telemetry_gps.h"
#include "telemetry_mpu.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void ui_init(void);
void ui_update(gps_data_t gps, mpu_data_t mpu, uint32_t cur_time, uint32_t last_lap, uint32_t best_lap, uint16_t laps, uint16_t race_id);
void ui_show_mode_splash(race_mode_t mode);
void ui_show_popup(const char *text, uint32_t duration_ms);
void ui_add_lap_to_list(uint16_t lap_num, uint32_t time_ms);
void ui_clear_lap_list(void);
void ui_refresh_session_dropdown(void);
void ui_add_point_to_chart(float speed);
void ui_update_reset_progress(uint32_t progress);
void ui_hide_reset_progress(void);

// Nova função para atualizar o status do armazenamento
void ui_update_sd_info(void);

#ifdef __cplusplus
}
#endif

#endif