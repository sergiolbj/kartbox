#include "ui_kartbox.h"
#include "lvgl.h"
#include "esp_lvgl_port.h"
#include "bsp/esp-bsp.h"
#include "telemetry_sd.h"
#include "telemetry_gps.h"
#include "usb_mode.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h" // <--- NOVO INCLUDE PARA REINICIAR
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// --- DECLARAÇÃO DE FONTES ---
LV_FONT_DECLARE(font_montserrat_bold_80);
LV_FONT_DECLARE(font_montserrat_bold_70);
LV_FONT_DECLARE(font_montserrat_medium_60);
LV_FONT_DECLARE(font_montserrat_60);
LV_FONT_DECLARE(font_montserrat_70);
LV_FONT_DECLARE(font_montserrat_80);

// --- VARIÁVEIS E FUNÇÕES EXTERNAS ---
extern bool recording_active;
extern void end_race_session(void);
extern int sd_get_session_string_list(char *buf, size_t max_len) __attribute__((weak));

// --- PALETA DE CORES ---
#define COLOR_BG        lv_color_hex(0x000000)
#define COLOR_PRIMARY   lv_color_hex(0x00FF41) // Verde Matrix
#define COLOR_SECONDARY lv_color_hex(0xFFFF00) // Amarelo Qualy
#define COLOR_DANGER    lv_color_hex(0xFF0033) // Vermelho Erro
#define COLOR_WARNING   lv_color_hex(0xFFCC00) // Laranja Busca
#define COLOR_TEXT      lv_color_hex(0xFFFFFF)
#define COLOR_PANEL     lv_color_hex(0x1A1A1A)
#define COLOR_GRAY      lv_color_hex(0x888888)

// --- OBJETOS GLOBAIS ---
static lv_obj_t *tabview, *list_laps = NULL, *dd_sessions = NULL, *lbl_sd_storage = NULL;
static lv_obj_t *lbl_speed, *lbl_lap_current, *lbl_lap_best, *lbl_lap_num, *lbl_gps_top, *lbl_mode, *lbl_race_name, *mode_border;
static lv_obj_t *lbl_delta, *ui_reset_bar = NULL, *ui_chart = NULL, *lbl_chart_max_val = NULL;
static lv_chart_series_t *ui_ser_speed = NULL;
static uint32_t ui_best_ms = 0xFFFFFFFF;

// Escala dinâmica do gráfico
static float ui_session_max_speed = 40.0f; 
static uint16_t ui_total_laps_in_chart = 0;

static lv_style_t style_text_white, style_list_btn, style_big_font;

// Flags de controle
static volatile bool ui_is_saving_task_running = false;
static bool ui_usb_mode_active = false; // <--- NOVA FLAG PARA O BOTÃO

// Variáveis para monitoramento de saúde do GPS
static uint32_t last_gps_packet_time = 0;
static uint32_t last_ui_tick_check = 0;

// --- PROTÓTIPOS OBRIGATÓRIOS ---
void ui_refresh_session_dropdown(void);
void ui_update_sd_info(void);
void ui_show_popup(const char *t, uint32_t d);
void ui_show_mode_splash(race_mode_t m); 
void ui_clear_lap_list(void);
void ui_hide_reset_progress(void);
void ui_update_reset_progress(uint32_t progress);

// --- FUNÇÕES AUXILIARES DE TEMPO ---

static void format_time(char *buf, size_t len, uint32_t ms) {
    if (ms == 0) { snprintf(buf, len, "00:00.000"); return; }
    snprintf(buf, len, "%02lu:%02lu.%03lu", ms/60000, (ms%60000)/1000, ms%1000);
}

static void delete_obj_timer_cb(lv_timer_t * t) {
    lv_obj_t * obj = (lv_obj_t *)lv_timer_get_user_data(t);
    if (lv_obj_is_valid(obj)) lv_obj_delete(obj);
}

// --- CALLBACKS DE EVENTOS ---

static void refresh_history_cb(lv_event_t * e) {
    if (ui_is_saving_task_running) return;

    lv_obj_t * dropdown = (lv_obj_t *)lv_event_get_user_data(e);
    uint16_t sel_idx = lv_dropdown_get_selected(dropdown);
    
    ui_clear_lap_list();
    ui_show_popup("RECARREGANDO...", 500);
    ui_refresh_session_dropdown();
    sd_load_session_history(sel_idx);
}

static void session_dropdown_cb(lv_event_t * e) {
    lv_obj_t * dropdown = lv_event_get_target(e);
    uint16_t sel_idx = lv_dropdown_get_selected(dropdown);
    ui_clear_lap_list();
    sd_load_session_history(sel_idx);
}

// --- CALLBACK DO BOTÃO USB (MODIFICADO) ---
static void btn_usb_cb(lv_event_t * e) {
    // CLIQUE 2: REINICIAR O SISTEMA
    if (ui_usb_mode_active) {
        ui_show_popup("REINICIANDO...", 5000);
        // Pequeno delay para a tela atualizar antes de morrer
        vTaskDelay(pdMS_TO_TICKS(500)); 
        esp_restart();
        return;
    }

    // CLIQUE 1: ATIVAR USB
    if (recording_active) {
        ui_show_popup("PARE A GRAVACAO!", 1500);
        return;
    }
    
    // Inicia o USB
    usb_mode_start();
    ui_usb_mode_active = true; // Marca que já ativou
    
    // Muda visual do botão
    lv_obj_t * btn = lv_event_get_target(e);
    lv_obj_set_style_bg_color(btn, COLOR_PRIMARY, 0); // Fica Verde
    
    lv_obj_t * label = lv_obj_get_child(btn, 0);
    lv_label_set_text(label, "REINICIAR SISTEMA (SAIR)"); // Muda o Texto
}

static void confirm_delete_cb(lv_event_t * e) {
    sd_delete_all_sessions();
    ui_refresh_session_dropdown();
    ui_clear_lap_list();
    ui_update_sd_info();
    ui_show_popup("SD LIMPO", 1500);
    lv_obj_delete(lv_obj_get_parent(lv_event_get_target(e)));
}

static void cancel_delete_cb(lv_event_t * e) { 
    lv_obj_delete(lv_obj_get_parent(lv_event_get_target(e)));
}

static void btn_delete_trigger_cb(lv_event_t * e) {
    lv_obj_t * modal = lv_obj_create(lv_layer_top());
    lv_obj_set_size(modal, 550, 280); lv_obj_center(modal);
    lv_obj_set_style_bg_color(modal, COLOR_PANEL, 0);
    lv_obj_set_style_border_color(modal, COLOR_DANGER, 0);
    lv_obj_set_style_border_width(modal, 5, 0);
    lv_obj_t * l = lv_label_create(modal);
    lv_label_set_text(l, "APAGAR TODAS AS CORRIDAS?");
    lv_obj_add_style(l, &style_text_white, 0);
    lv_obj_align(l, LV_ALIGN_TOP_MID, 0, 30);
    lv_obj_t * b1 = lv_button_create(modal); lv_obj_set_size(b1, 200, 60);
    lv_obj_align(b1, LV_ALIGN_BOTTOM_LEFT, 30, -30); lv_obj_set_style_bg_color(b1, COLOR_DANGER, 0);
    lv_obj_add_event_cb(b1, confirm_delete_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t * lt1 = lv_label_create(b1); lv_label_set_text(lt1, "SIM"); lv_obj_center(lt1);
    lv_obj_t * b2 = lv_button_create(modal); lv_obj_set_size(b2, 200, 60);
    lv_obj_align(b2, LV_ALIGN_BOTTOM_RIGHT, -30, -30);
    lv_obj_set_style_bg_color(b2, lv_color_hex(0x444444), 0);
    lv_obj_add_event_cb(b2, cancel_delete_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t * lt2 = lv_label_create(b2); lv_label_set_text(lt2, "NAO"); lv_obj_center(lt2);
}

// --- TAREFA DEDICADA PARA SALVAR ---

static void ui_notify_save_finished(void * arg) {
    ui_show_popup("SESSAO SALVA", 1500);
    ui_is_saving_task_running = false; 
    ui_refresh_session_dropdown(); 
}

static void save_session_worker_task(void * arg) {
    if (recording_active) {
        end_race_session(); 
    }
    lv_async_call(ui_notify_save_finished, NULL);
    vTaskDelete(NULL);
}

static void digital_btn_cb(lv_event_t * e) {
    if (ui_is_saving_task_running) return;

    uintptr_t type = (uintptr_t)lv_event_get_user_data(e);
    lv_event_code_t code = lv_event_get_code(e);
    
    static uint32_t press_start_tick = 0;
    static bool long_press_triggered = false;

    if(type == 2) { // RESET
        if(code == LV_EVENT_PRESSED) {
            press_start_tick = lv_tick_get();
            long_press_triggered = false;
        } 
        else if(code == LV_EVENT_PRESSING) {
            if(long_press_triggered) return;
            uint32_t elapsed = lv_tick_elaps(press_start_tick);

            if(elapsed >= 2000) { 
                long_press_triggered = true;
                ui_hide_reset_progress();
                
                if (recording_active) {
                    ui_is_saving_task_running = true; 
                    xTaskCreate(save_session_worker_task, "SaveTask", 4096, NULL, 5, NULL);
                } else {
                    ui_show_popup("SEM CORRIDA ATIVA", 1000);
                }
            } else {
                ui_update_reset_progress((elapsed * 100) / 2000);
            }
        }
        else if(code == LV_EVENT_RELEASED) {
            uint32_t elapsed = lv_tick_elaps(press_start_tick);
            ui_hide_reset_progress();

            if(!long_press_triggered && elapsed < 500) {
                if(!recording_active) {
                    gps_reset_session(); 
                    ui_clear_lap_list(); 
                    ui_show_popup("ZERADO", 1000);
                } else {
                    ui_show_popup("SEGURE 2S PARA SALVAR", 1500);
                }
            }
            press_start_tick = 0;
        }
        else if(code == LV_EVENT_PRESS_LOST) {
            ui_hide_reset_progress();
            press_start_tick = 0;
        }
    } 
    else if(code == LV_EVENT_CLICKED) { // MODOS
        if(type == 0) { 
            gps_toggle_mode();
            ui_show_mode_splash(gps_get_mode());
        } else if(type == 1) {
            if(gps_set_finish_line()) {
                recording_active = true;
                ui_show_popup("LINHA MARCADA", 1200);
            } else {
                ui_show_popup("ERRO: SEM GPS", 1500);
            }
        }
    }
}

// --- INICIALIZAÇÃO DA INTERFACE ---

void ui_init(void) {
    lv_style_init(&style_text_white);
    lv_style_set_text_color(&style_text_white, COLOR_TEXT);
    
    lv_style_init(&style_big_font);
    lv_style_set_text_color(&style_big_font, COLOR_TEXT);
    lv_style_set_text_opa(&style_big_font, LV_OPA_COVER);
    lv_style_set_bg_opa(&style_big_font, LV_OPA_TRANSP); 

    lv_style_init(&style_list_btn);
    lv_style_set_bg_color(&style_list_btn, COLOR_PANEL);
    lv_style_set_text_color(&style_list_btn, COLOR_TEXT);
    lv_style_set_border_width(&style_list_btn, 0);

    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, COLOR_BG, 0);

    tabview = lv_tabview_create(scr);
    lv_tabview_set_tab_bar_position(tabview, LV_DIR_BOTTOM);
    lv_tabview_set_tab_bar_size(tabview, 65);
    lv_obj_set_style_bg_color(tabview, COLOR_BG, 0);
    lv_obj_set_style_border_width(tabview, 0, 0);

    lv_obj_t * tab_bar = lv_tabview_get_tab_bar(tabview);
    lv_obj_set_style_bg_color(tab_bar, COLOR_PANEL, 0);
    lv_obj_set_style_text_color(tab_bar, lv_color_hex(0x888888), 0); 
    lv_obj_set_style_text_font(tab_bar, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(tab_bar, COLOR_PRIMARY, LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(tab_bar, COLOR_PRIMARY, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(tab_bar, LV_OPA_COVER, LV_PART_INDICATOR);

    lv_obj_t *t1 = lv_tabview_add_tab(tabview, "RACE");
    lv_obj_t *t2 = lv_tabview_add_tab(tabview, "VOLTAS");
    lv_obj_t *t3 = lv_tabview_add_tab(tabview, "CFG");

    lv_obj_set_style_bg_color(t1, COLOR_BG, 0);
    lv_obj_set_style_pad_all(t1, 0, 0);

    // --- ABA 1: RACE ---
    lbl_speed = lv_label_create(t1);
    lv_obj_add_style(lbl_speed, &style_big_font, 0);
    lv_obj_set_style_text_font(lbl_speed, &font_montserrat_bold_80, 0); 
    lv_obj_align(lbl_speed, LV_ALIGN_TOP_MID, 0, 10); 
    lv_label_set_text(lbl_speed, "0 KM/H");

    lbl_lap_current = lv_label_create(t1);
    lv_obj_add_style(lbl_lap_current, &style_big_font, 0);
    lv_obj_set_style_text_font(lbl_lap_current, &font_montserrat_bold_70, 0); 
    lv_obj_align(lbl_lap_current, LV_ALIGN_TOP_MID, 0, 130); 
    lv_label_set_text(lbl_lap_current, "00:00.000");

    lbl_delta = lv_label_create(t1);
    lv_obj_add_style(lbl_delta, &style_big_font, 0);
    lv_obj_set_style_text_font(lbl_delta, &font_montserrat_medium_60, 0); 
    lv_obj_align(lbl_delta, LV_ALIGN_TOP_MID, 0, 215); 
    lv_label_set_text(lbl_delta, "0.00");

    lbl_lap_num = lv_label_create(t1);
    lv_obj_add_style(lbl_lap_num, &style_text_white, 0);
    lv_obj_set_style_text_font(lbl_lap_num, &lv_font_montserrat_32, 0); 
    lv_obj_align(lbl_lap_num, LV_ALIGN_TOP_MID, -180, 295); 
    lv_label_set_text(lbl_lap_num, "VOLTA 0");

    lbl_lap_best = lv_label_create(t1);
    lv_obj_add_style(lbl_lap_best, &style_text_white, 0);
    lv_obj_set_style_text_font(lbl_lap_best, &lv_font_montserrat_32, 0); 
    lv_obj_align(lbl_lap_best, LV_ALIGN_TOP_MID, 130, 295); 
    lv_label_set_text(lbl_lap_best, "BEST: --:--.---");

    lbl_gps_top = lv_label_create(t1);
    lv_obj_add_style(lbl_gps_top, &style_text_white, 0);
    lv_obj_set_style_text_font(lbl_gps_top, &lv_font_montserrat_18, 0);
    lv_obj_align(lbl_gps_top, LV_ALIGN_TOP_RIGHT, -25, 10);

    lbl_mode = lv_label_create(t1);
    lv_obj_add_style(lbl_mode, &style_text_white, 0);
    lv_obj_set_style_text_font(lbl_mode, &lv_font_montserrat_18, 0);
    lv_obj_align(lbl_mode, LV_ALIGN_TOP_LEFT, 25, 10);

    lbl_race_name = lv_label_create(t1);
    lv_obj_add_style(lbl_race_name, &style_text_white, 0);
    lv_obj_set_style_text_font(lbl_race_name, &lv_font_montserrat_18, 0);
    lv_obj_align(lbl_race_name, LV_ALIGN_TOP_LEFT, 25, 35);

    lv_obj_t *btn_cont = lv_obj_create(t1);
    lv_obj_set_size(btn_cont, 800, 80);
    lv_obj_align(btn_cont, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_set_flex_flow(btn_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(btn_cont, 0, 0);
    lv_obj_set_style_border_width(btn_cont, 0, 0);
    lv_obj_set_style_pad_column(btn_cont, 15, 0); 

    const char *btn_names[] = {"MODO", "MARCAR", "RESET"};
    lv_color_t btn_colors[] = {COLOR_SECONDARY, COLOR_PRIMARY, COLOR_DANGER};
    for(int i=0; i<3; i++) {
        lv_obj_t *b = lv_button_create(btn_cont);
        lv_obj_set_size(b, 230, 60);
        if(i == 2) lv_obj_add_event_cb(b, digital_btn_cb, LV_EVENT_ALL, (void*)(uintptr_t)i);
        else lv_obj_add_event_cb(b, digital_btn_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)i);
        lv_obj_set_style_bg_color(b, btn_colors[i], 0);
        lv_obj_t *l = lv_label_create(b);
        lv_label_set_text(l, btn_names[i]);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_24, 0); 
        lv_obj_center(l);
        if(i == 0 || i == 1) lv_obj_set_style_text_color(l, COLOR_BG, 0);
        else lv_obj_set_style_text_color(l, COLOR_TEXT, 0);
    }

    mode_border = lv_obj_create(t1);
    lv_obj_set_size(mode_border, 800, 415);
    lv_obj_align(mode_border, LV_ALIGN_TOP_MID, 0, -10);
    lv_obj_set_style_bg_opa(mode_border, 0, 0); 
    lv_obj_set_style_border_width(mode_border, 12, 0);
    lv_obj_set_style_radius(mode_border, 0, 0);
    lv_obj_remove_flag(mode_border, LV_OBJ_FLAG_CLICKABLE); 
    lv_obj_add_flag(mode_border, LV_OBJ_FLAG_IGNORE_LAYOUT);

    // --- ABA 2: VOLTAS ---
    
    dd_sessions = lv_dropdown_create(t2);
    lv_obj_set_size(dd_sessions, 230, 45); 
    lv_obj_align(dd_sessions, LV_ALIGN_TOP_LEFT, 20, 10);
    lv_obj_set_style_bg_color(dd_sessions, COLOR_PANEL, 0);
    lv_obj_add_style(dd_sessions, &style_text_white, 0);
    lv_obj_add_event_cb(dd_sessions, session_dropdown_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t * btn_refresh = lv_button_create(t2);
    lv_obj_set_size(btn_refresh, 60, 45);
    lv_obj_align_to(btn_refresh, dd_sessions, LV_ALIGN_OUT_RIGHT_MID, 10, 0);
    lv_obj_set_style_bg_color(btn_refresh, lv_color_hex(0x333333), 0);
    lv_obj_add_event_cb(btn_refresh, refresh_history_cb, LV_EVENT_CLICKED, dd_sessions);
    lv_obj_t * l_ref = lv_label_create(btn_refresh);
    lv_label_set_text(l_ref, LV_SYMBOL_REFRESH);
    lv_obj_center(l_ref);
    lv_obj_set_style_text_color(l_ref, COLOR_TEXT, 0);

    ui_chart = lv_chart_create(t2);
    lv_obj_set_size(ui_chart, 360, 140);
    lv_obj_align(ui_chart, LV_ALIGN_TOP_RIGHT, -50, 10);
    lv_chart_set_type(ui_chart, LV_CHART_TYPE_BAR);
    lv_chart_set_range(ui_chart, LV_CHART_AXIS_PRIMARY_Y, 0, (int32_t)ui_session_max_speed);
    
    // Divisões visuais
    lv_chart_set_div_line_count(ui_chart, 5, 10); 

    ui_ser_speed = lv_chart_add_series(ui_chart, COLOR_PRIMARY, LV_CHART_AXIS_PRIMARY_Y);
    lv_obj_set_style_bg_color(ui_chart, COLOR_PANEL, 0);

    lbl_chart_max_val = lv_label_create(t2); 
    char tmp[16]; snprintf(tmp, 16, "%d", (int)ui_session_max_speed);
    lv_label_set_text(lbl_chart_max_val, tmp); 
    lv_obj_add_style(lbl_chart_max_val, &style_text_white, 0); 
    lv_obj_align_to(lbl_chart_max_val, ui_chart, LV_ALIGN_OUT_LEFT_TOP, -5, 0);

    lv_obj_t *lu = lv_label_create(t2); lv_label_set_text(lu, "KM/H");
    lv_obj_add_style(lu, &style_text_white, 0); lv_obj_align_to(lu, ui_chart, LV_ALIGN_OUT_LEFT_BOTTOM, -5, 0);

    list_laps = lv_list_create(t2);
    lv_obj_set_size(list_laps, 760, 200);
    lv_obj_align(list_laps, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_bg_color(list_laps, COLOR_BG, 0);

    // --- ABA 3: CFG ---
    lbl_sd_storage = lv_label_create(t3);
    lv_obj_add_style(lbl_sd_storage, &style_text_white, 0);
    lv_obj_set_style_text_font(lbl_sd_storage, &lv_font_montserrat_32, 0);
    lv_obj_align(lbl_sd_storage, LV_ALIGN_TOP_MID, 0, 60);

    // --- BOTÃO USB (COM FUNÇÃO DE REINICIAR) ---
    lv_obj_t * b_usb = lv_button_create(t3);
    lv_obj_set_size(b_usb, 450, 70);
    lv_obj_align(b_usb, LV_ALIGN_CENTER, 0, -20); // Centralizado verticalmente
    lv_obj_set_style_bg_color(b_usb, lv_color_hex(0x444444), 0);
    lv_obj_add_event_cb(b_usb, btn_usb_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t * lt_usb = lv_label_create(b_usb); 
    lv_label_set_text(lt_usb, "MODO PEN DRIVE (USB)"); 
    lv_obj_center(lt_usb);

    // --- BOTÃO APAGAR TUDO ---
    lv_obj_t * b_del = lv_button_create(t3);
    lv_obj_set_size(b_del, 450, 70);
    lv_obj_align(b_del, LV_ALIGN_CENTER, 0, 80); // Movido para baixo
    lv_obj_set_style_bg_color(b_del, COLOR_DANGER, 0);
    lv_obj_add_event_cb(b_del, btn_delete_trigger_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t * lt_del = lv_label_create(b_del); lv_label_set_text(lt_del, "APAGAR TUDO (SD CARD)"); lv_obj_center(lt_del);
}

// --- FUNÇÃO UI UPDATE COM DIAGNÓSTICO DE CORES ---

void ui_update(gps_data_t gps, mpu_data_t mpu, uint32_t cur_time, uint32_t last_lap, uint32_t best_lap, uint16_t laps, uint16_t race_id) {
    if (ui_is_saving_task_running) return;

    static char buf[64];

    // 1. Diagnóstico de Hardware (Heartbeat)
    bool hardware_ok = false;
    
    if (gps.timestamp_ms != last_gps_packet_time) {
        last_gps_packet_time = gps.timestamp_ms;
        last_ui_tick_check = lv_tick_get();
        hardware_ok = true;
    } else {
        if (lv_tick_elaps(last_ui_tick_check) < 2500) {
            hardware_ok = true;
        }
    }

    snprintf(buf, sizeof(buf), "%d KM/H", (int)gps.speed_kmh); lv_label_set_text(lbl_speed, buf);
    snprintf(buf, sizeof(buf), "VOLTA %u", laps); lv_label_set_text(lbl_lap_num, buf);
    format_time(buf, sizeof(buf), cur_time); lv_label_set_text(lbl_lap_current, buf);
    
    if (best_lap > 0) {
        char t_buf[16]; format_time(t_buf, 16, best_lap);
        snprintf(buf, sizeof(buf), "BEST: %s", t_buf); lv_label_set_text(lbl_lap_best, buf);
        int32_t delta = gps_get_live_delta();
        snprintf(buf, sizeof(buf), "%+.2f", delta / 1000.0f);
        lv_label_set_text(lbl_delta, buf);
        lv_obj_set_style_text_color(lbl_delta, (delta <= 0) ? COLOR_PRIMARY : COLOR_DANGER, 0);
    }

    bool q = (gps_get_mode() == MODE_CLASSIFICACAO);
    lv_label_set_text(lbl_mode, q ? "MODO: QUALY" : "MODO: RACE");
    lv_obj_set_style_border_color(mode_border, q ? COLOR_SECONDARY : COLOR_PRIMARY, 0);

    // 2. Atualiza Status do GPS com Cores Personalizadas
    if (!hardware_ok) {
        lv_label_set_text(lbl_gps_top, "GPS: ERRO HW"); 
        lv_obj_set_style_text_color(lbl_gps_top, COLOR_DANGER, 0); // VERMELHO
    } 
    else if (gps.valid) {
        snprintf(buf, sizeof(buf), "SATS: %d FIX", gps.sats);
        lv_label_set_text(lbl_gps_top, buf);
        lv_obj_set_style_text_color(lbl_gps_top, COLOR_PRIMARY, 0); // VERDE
    } 
    else {
        snprintf(buf, sizeof(buf), "SATS: %d BUSCA...", gps.sats);
        lv_label_set_text(lbl_gps_top, buf);
        lv_obj_set_style_text_color(lbl_gps_top, COLOR_WARNING, 0); // LARANJA
    }

    snprintf(buf, sizeof(buf), "CORRIDA %u", race_id);
    lv_label_set_text(lbl_race_name, buf);
}

void ui_add_point_to_chart(float speed) {
    if (ui_chart && ui_ser_speed) {
        if (speed > ui_session_max_speed) {
            ui_session_max_speed = speed + 10.0f;
            lv_chart_set_range(ui_chart, LV_CHART_AXIS_PRIMARY_Y, 0, (int32_t)ui_session_max_speed);
            if (lbl_chart_max_val) {
                char b[16]; snprintf(b, 16, "%d", (int)ui_session_max_speed);
                lv_label_set_text(lbl_chart_max_val, b);
            }
        }
        
        lv_chart_set_next_value(ui_chart, ui_ser_speed, (int32_t)speed);
        ui_total_laps_in_chart++;
        lv_chart_set_div_line_count(ui_chart, 5, ui_total_laps_in_chart > 0 ? ui_total_laps_in_chart : 1);
        lv_chart_refresh(ui_chart);
    }
}

void ui_clear_lap_list(void) { 
    if (list_laps) lv_obj_clean(list_laps); 
    if (ui_chart) {
        ui_session_max_speed = 40.0f;
        ui_total_laps_in_chart = 0;
        lv_chart_set_all_value(ui_chart, ui_ser_speed, LV_CHART_POINT_NONE);
        lv_chart_set_range(ui_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 40);
        lv_chart_set_div_line_count(ui_chart, 5, 1);
        if (lbl_chart_max_val) lv_label_set_text(lbl_chart_max_val, "40");
    }
    ui_best_ms = 0xFFFFFFFF; 
}

void ui_update_reset_progress(uint32_t progress) {
    if(ui_reset_bar == NULL) {
        ui_reset_bar = lv_bar_create(lv_layer_top());
        lv_obj_set_size(ui_reset_bar, 300, 25);
        lv_obj_align(ui_reset_bar, LV_ALIGN_CENTER, 0, 50);
        lv_obj_set_style_bg_color(ui_reset_bar, COLOR_DANGER, LV_PART_INDICATOR);
        lv_obj_t * label = lv_label_create(ui_reset_bar);
        lv_label_set_text(label, "A SALVAR...");
        lv_obj_center(label);
        lv_obj_set_style_text_color(label, COLOR_TEXT, 0);
    }
    lv_bar_set_value(ui_reset_bar, progress, LV_ANIM_ON);
}

void ui_hide_reset_progress(void) { if(ui_reset_bar) { lv_obj_delete(ui_reset_bar); ui_reset_bar = NULL; } }

void ui_add_lap_to_list(uint16_t num, uint32_t ms) {
    if (!list_laps) return;
    char b[128], t[16]; format_time(t, 16, ms); snprintf(b, sizeof(b), "Volta %d: %s", num, t);
    lv_obj_t *btn = lv_list_add_button(list_laps, LV_SYMBOL_PLAY, b);
    lv_obj_add_style(btn, &style_list_btn, 0);
}

void ui_refresh_session_dropdown(void) {
    if (!dd_sessions) return;
    
    if (sd_get_session_string_list) {
        char *buf = malloc(4096); 
        if (buf) {
            if (sd_get_session_string_list(buf, 4096) > 0) {
                lv_dropdown_set_options(dd_sessions, buf);
            } else {
                lv_dropdown_set_options(dd_sessions, "Vazio");
            }
            free(buf);
            return;
        }
    }

    uint16_t sessions[50]; int count = sd_get_available_sessions(sessions, 50);
    if (count == 0) { lv_dropdown_set_options(dd_sessions, "Vazio"); return; }
    char *buf = malloc(4096); buf[0] = '\0';
    for (int i = 0; i < count; i++) { 
        char t[64]; snprintf(t, 64, "SESSION_%03d.LOG\n", sessions[i]); strcat(buf, t);
    }
    lv_dropdown_set_options(dd_sessions, buf);
    free(buf);
}

void ui_update_sd_info(void) {
    if (!lbl_sd_storage) return;
    float used, total;
    sd_get_info(&used, &total);
    static char buf[128];
    if (total > 0) snprintf(buf, sizeof(buf), "SD: %.2fGB / %.2fGB", used, total);
    else snprintf(buf, sizeof(buf), "SD: NAO DETECTADO");
    lv_label_set_text(lbl_sd_storage, buf);
}

void ui_show_mode_splash(race_mode_t m) {
    lv_obj_t *obj = lv_obj_create(lv_layer_top());
    lv_obj_set_size(obj, 800, 480); lv_obj_center(obj);
    lv_obj_set_style_bg_color(obj, m == MODE_CLASSIFICACAO ? COLOR_SECONDARY : COLOR_PRIMARY, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_90, 0);
    lv_obj_t *l = lv_label_create(obj);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(l, COLOR_BG, 0);
    lv_label_set_text(l, m == MODE_CLASSIFICACAO ? "MODO QUALY" : "MODO CORRIDA");
    lv_obj_center(l);
    lv_timer_t * t = lv_timer_create(delete_obj_timer_cb, 1000, obj);
    lv_timer_set_repeat_count(t, 1);
}

void ui_show_popup(const char *t, uint32_t d) {
    lv_obj_t *p = lv_obj_create(lv_layer_top()); 
    lv_obj_set_size(p, 450, 160); lv_obj_center(p);
    lv_obj_set_style_bg_color(p, COLOR_PANEL, 0);
    lv_obj_t *l = lv_label_create(p);
    lv_label_set_text(l, t); lv_obj_add_style(l, &style_text_white, 0); lv_obj_center(l);
    lv_timer_t * timer = lv_timer_create(delete_obj_timer_cb, d, p);
    lv_timer_set_repeat_count(timer, 1);
}