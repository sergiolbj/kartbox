#ifndef TELEMETRY_GPS_H
#define TELEMETRY_GPS_H

#include <stdint.h>
#include <stdbool.h>

typedef enum { 
    GPS_STATUS_DISCONNECTED, 
    GPS_STATUS_OFF,          
    GPS_STATUS_SEARCHING,    
    GPS_STATUS_FIXED         
} gps_status_t;

typedef enum { MODE_CLASSIFICACAO, MODE_CORRIDA } race_mode_t;

typedef struct {
    float lat; float lon; float speed_kmh; uint32_t timestamp_ms;
    float course; // Direção atual em graus (0-359)
    int sats; 
    bool valid;
    uint8_t day, month, year;
    uint8_t hour, minute, second;
} gps_data_t;

typedef struct { 
    float lat; 
    float lon; 
    float heading; // Direção da linha de chegada
    bool defined; 
} finish_line_t;

void gps_init(void);
gps_data_t gps_get_latest(void);
void gps_process_timing(gps_data_t *data);
bool gps_set_finish_line(void);
void gps_toggle_mode(void);
race_mode_t gps_get_mode(void);
gps_status_t gps_get_status(void);
bool gps_is_communicating(void);
uint32_t gps_get_current_time_ms(void);
uint32_t gps_get_last_lap(void);
uint32_t gps_get_best_lap(void);
uint16_t gps_get_lap_count(void);
void gps_reset_session(void);
int32_t gps_get_live_delta(void);

#endif