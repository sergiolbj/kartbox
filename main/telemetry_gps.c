#include "telemetry_gps.h"
#include "config.h"
#include "telemetry_sd.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "esp_timer.h"
#include "driver/uart.h"
#include "esp_log.h"

static const char *TAG = "GPS_DRV";
static gps_data_t last_gps = {0};
static race_mode_t mode = MODE_CLASSIFICACAO;
static finish_line_t f_line = {0};
static uint64_t last_cross_us = 0;
static uint32_t best_ms = 0, last_ms = 0;
static uint16_t laps = 0;
static bool inside = false;
static char line_buffer[1024];
static int line_pos = 0;
static uint32_t last_uart_rx_ms = 0;
static float speed_sum = 0;
static uint32_t speed_samples = 0;

// Flag para disparar cronômetro apenas no movimento no modo RACE
static bool race_waiting_for_movement = false;

const uint8_t UBX_10HZ[] = {0xB5, 0x62, 0x06, 0x08, 0x06, 0x00, 0x64, 0x00, 0x01, 0x00, 0x01, 0x00, 0x7A, 0x12};

void gps_init(void) {
    const uart_config_t cfg = { .baud_rate = GPS_BAUD_RATE, .data_bits = UART_DATA_8_BITS, .parity = UART_PARITY_DISABLE, .stop_bits = UART_STOP_BITS_1, .source_clk = UART_SCLK_DEFAULT };
    uart_driver_install(GPS_UART_NUM, 2048, 0, 0, NULL, 0);
    uart_param_config(GPS_UART_NUM, &cfg);
    uart_set_pin(GPS_UART_NUM, GPS_TX_PIN, GPS_RX_PIN, -1, -1);
    uart_write_bytes(GPS_UART_NUM, (const char*)UBX_10HZ, sizeof(UBX_10HZ));
}

static void parse_nmea(const char *s) {
    if (strstr(s, "GGA")) {
        char *p = strdup(s), *tok, *save; int f = 0;
        for (tok = strtok_r(p, ",", &save); tok; tok = strtok_r(NULL, ",", &save), f++) {
            if (f == 2 && strlen(tok) > 0) { float raw = atof(tok); float deg = (int)(raw/100); last_gps.lat = deg + (raw-deg*100)/60.0f; }
            else if (f == 3 && tok[0] == 'S') last_gps.lat *= -1;
            else if (f == 4 && strlen(tok) > 0) { float raw = atof(tok); float deg = (int)(raw/100); last_gps.lon = deg + (raw-deg*100)/60.0f; }
            else if (f == 5 && tok[0] == 'W') last_gps.lon *= -1;
            else if (f == 6) last_gps.valid = (tok[0] != '0');
            else if (f == 7) last_gps.sats = atoi(tok);
        }
        free(p);
    } else if (strstr(s, "RMC")) {
        char *p = strdup(s), *tok, *save; int f = 0;
        for (tok = strtok_r(p, ",", &save); tok; tok = strtok_r(NULL, ",", &save), f++) {
            if (f == 1 && strlen(tok) >= 6) {
                int h = (tok[0]-'0')*10 + (tok[1]-'0');
                h = (h < 3) ? h + 21 : h - 3; 
                last_gps.hour = h;
                last_gps.minute = (tok[2]-'0')*10 + (tok[3]-'0');
                last_gps.second = (tok[4]-'0')*10 + (tok[5]-'0');
            }
            else if (f == 2) last_gps.valid = (tok[0] == 'A');
            else if (f == 7 && strlen(tok) > 0) last_gps.speed_kmh = atof(tok) * 1.852f;
            else if (f == 8 && strlen(tok) > 0) last_gps.course = atof(tok); 
            else if (f == 9 && strlen(tok) >= 6) {
                last_gps.day = (tok[0]-'0')*10 + (tok[1]-'0');
                last_gps.month = (tok[2]-'0')*10 + (tok[3]-'0');
                last_gps.year = (tok[4]-'0')*10 + (tok[5]-'0');
            }
        }
        free(p);
    }
}

gps_data_t gps_get_latest(void) {
    uint8_t buf[512];
    int len = uart_read_bytes(GPS_UART_NUM, buf, sizeof(buf)-1, pdMS_TO_TICKS(5));
    if (len > 0) {
        last_uart_rx_ms = esp_timer_get_time() / 1000;
        for (int i = 0; i < len; i++) {
            if (buf[i] == '\n') { line_buffer[line_pos] = '\0'; parse_nmea(line_buffer); line_pos = 0; }
            else if (line_pos < 1023) line_buffer[line_pos++] = buf[i];
        }
    }
    last_gps.timestamp_ms = esp_timer_get_time() / 1000;
    return last_gps;
}

bool gps_is_communicating(void) { return (esp_timer_get_time() / 1000 - last_uart_rx_ms) < 2000; }
gps_status_t gps_get_status(void) {
    if (!gps_is_communicating()) return GPS_STATUS_DISCONNECTED;
    if (last_gps.valid) return GPS_STATUS_FIXED;
    return (last_gps.sats > 0) ? GPS_STATUS_SEARCHING : GPS_STATUS_OFF;
}

bool gps_set_finish_line(void) {
    if (!last_gps.valid) return false;
    f_line.lat = last_gps.lat; 
    f_line.lon = last_gps.lon; 
    f_line.heading = last_gps.course; 
    f_line.defined = true;
    last_cross_us = esp_timer_get_time();
    sd_start_new_session(last_gps); 
    return true;
}

void gps_process_timing(gps_data_t *d) {
    if (!d->valid || !f_line.defined) return;

    // Se estivermos esperando o movimento para largada no modo RACE
    if (mode == MODE_CORRIDA && race_waiting_for_movement) {
        if (d->speed_kmh > 5.0f) {
            last_cross_us = esp_timer_get_time();
            race_waiting_for_movement = false;
            ESP_LOGI(TAG, "Largada detectada! Cronômetro iniciado.");
        }
        return; 
    }

    if (d->speed_kmh > 1.0) { speed_sum += d->speed_kmh; speed_samples++; }

    float dist = sqrtf(powf(111320.0f*(d->lat-f_line.lat),2)+powf(111320.0f*cosf(d->lat*0.0174f)*(d->lon-f_line.lon),2));
    float diff_heading = fabsf(d->course - f_line.heading);
    if (diff_heading > 180) diff_heading = 360 - diff_heading;

    if (dist < GATE_RADIUS_M && diff_heading < 45.0f) {
        if (!inside) {
            uint64_t now = esp_timer_get_time();
            uint32_t diff = (uint32_t)((now - last_cross_us) / 1000);
            if (diff > MIN_LAP_TIME_MS) {
                last_ms = diff; laps++;
                if (best_ms == 0 || diff < best_ms) best_ms = diff;
                float avg_speed = (speed_samples > 0) ? (speed_sum / speed_samples) : d->speed_kmh;
                sd_save_lap_event(laps, diff, avg_speed, *d, mode);
                speed_sum = 0; speed_samples = 0; last_cross_us = now;
            }
            inside = true;
        }
    } else inside = false;
}

uint32_t gps_get_current_time_ms(void) { 
    if (last_cross_us == 0) return 0;
    return (uint32_t)((esp_timer_get_time() - last_cross_us)/1000); 
}

uint32_t gps_get_last_lap(void) { return last_ms; }
uint32_t gps_get_best_lap(void) { return best_ms; }
uint16_t gps_get_lap_count(void) { return laps; }

void gps_reset_session(void) { 
    laps = 0; 
    best_ms = 0; 
    last_ms = 0; 
    speed_sum = 0; 
    speed_samples = 0; 
    last_cross_us = 0;
    f_line.defined = false;
    mode = MODE_CLASSIFICACAO; 
    race_waiting_for_movement = false;
    ESP_LOGI(TAG, "Sessão encerrada e resetada.");
}

race_mode_t gps_get_mode(void) { return mode; }

void gps_toggle_mode(void) { 
    mode = (mode == MODE_CLASSIFICACAO) ? MODE_CORRIDA : MODE_CLASSIFICACAO; 
    if (mode == MODE_CORRIDA) {
        race_waiting_for_movement = true;
        last_cross_us = 0; // Trava cronômetro visual até largada
    } else {
        race_waiting_for_movement = false;
    }
}

int32_t gps_get_live_delta(void) {
    if (best_ms == 0 || last_cross_us == 0) return 0;
    uint32_t current_lap_time = (esp_timer_get_time() - last_cross_us) / 1000;
    return (int32_t)current_lap_time - (int32_t)best_ms;
}