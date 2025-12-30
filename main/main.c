#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "bsp/esp-bsp.h"
#include "esp_lvgl_port.h"
#include "config.h"
#include "telemetry_gps.h"
#include "telemetry_sd.h"
#include "ui_kartbox.h"

bool recording_active = false; 
static uint32_t reset_press_start = 0;
static int last_mode_state = 1;
static int last_line_state = 1;

void end_race_session(void) {
    if (recording_active) {
        sd_stop_session();
        recording_active = false; 
        gps_reset_session();
        if (lvgl_port_lock(0)) {
            ui_clear_lap_list(); 
            ui_show_popup("SESSAO SALVA", 2500); 
            ui_hide_reset_progress();
            ui_update_sd_info(); // Atualiza o uso após salvar o arquivo
            lvgl_port_unlock();
        }
    }
}

void app_main(void) {
    bsp_display_start();
    if (lvgl_port_lock(0)) { 
        lv_display_set_rotation(lv_display_get_default(), LV_DISPLAY_ROTATION_270); 
        ui_init(); 
        lvgl_port_unlock(); 
    }
    bsp_display_backlight_on();

    gpio_config_t b_cfg = { 
        .pin_bit_mask = (1ULL<<BTN_MODE_PIN)|(1ULL<<BTN_SETLINE_PIN)|(1ULL<<BTN_RESET_PIN), 
        .mode = GPIO_MODE_INPUT, .pull_up_en = 1 
    };
    gpio_config(&b_cfg);

    gps_init();
    
    // Inicializa o SD e atualiza a interface se montado
    if (sd_init()) {
        if (lvgl_port_lock(0)) {
            ui_update_sd_info(); // MOSTRA O TAMANHO REAL DO CARTÃO AQUI
            ui_refresh_session_dropdown();
            lvgl_port_unlock();
        }
    }

    uint32_t last_ui = 0;

    while (1) {
        gps_data_t cur = gps_get_latest();
        gps_process_timing(&cur);
        
        if (recording_active) {
            sd_log_sample(cur, (mpu_data_t){0}, gps_get_mode(), gps_get_lap_count());
        }

        // --- BOTÃO MODO ---
        int mode_val = gpio_get_level(BTN_MODE_PIN);
        if (last_mode_state == 1 && mode_val == 0) {
            gps_toggle_mode();
            if (lvgl_port_lock(0)) { ui_show_mode_splash(gps_get_mode()); lvgl_port_unlock(); }
        }
        last_mode_state = mode_val;

        // --- BOTÃO LINHA ---
        int line_val = gpio_get_level(BTN_SETLINE_PIN);
        if (last_line_state == 1 && line_val == 0) {
            if (gps_set_finish_line()) {
                recording_active = true;
                if (lvgl_port_lock(0)) { ui_show_popup("GRAVANDO...", 1500); lvgl_port_unlock(); }
            } else {
                if (lvgl_port_lock(0)) { ui_show_popup("SEM SINAL GPS", 1500); lvgl_port_unlock(); }
            }
        }
        last_line_state = line_val;

        // --- BOTÃO RESET ---
        if (gpio_get_level(BTN_RESET_PIN) == 0) {
            if (reset_press_start == 0) reset_press_start = esp_timer_get_time() / 1000;
            uint32_t held = (esp_timer_get_time() / 1000) - reset_press_start;
            if (recording_active && lvgl_port_lock(0)) { ui_update_reset_progress((held * 100) / 2000); lvgl_port_unlock(); }
            if (held >= 2000) end_race_session();
        } else {
            if (reset_press_start != 0) {
                uint32_t held = (esp_timer_get_time() / 1000) - reset_press_start;
                if (held < 2000 && !recording_active) {
                    gps_reset_session();
                    if (lvgl_port_lock(0)) { ui_show_popup("ZERADO", 1000); lvgl_port_unlock(); }
                }
                if (lvgl_port_lock(0)) { ui_hide_reset_progress(); lvgl_port_unlock(); }
                reset_press_start = 0;
            }
        }

        uint32_t now = esp_timer_get_time() / 1000;
        if (now - last_ui >= UI_UPDATE_MS) {
            if (lvgl_port_lock(0)) {
                ui_update(cur, (mpu_data_t){0}, gps_get_current_time_ms(), gps_get_last_lap(), gps_get_best_lap(), gps_get_lap_count(), sd_get_current_session_id());
                lvgl_port_unlock();
            }
            last_ui = now;
        }
        vTaskDelay(pdMS_TO_TICKS(10)); 
    }
}