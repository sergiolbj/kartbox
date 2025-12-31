#include "usb_mode.h"
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/sdmmc_host.h"
#include "esp_lvgl_port.h"

// Includes do TinyUSB
#if __has_include("esp_tinyusb.h")
    #include "esp_tinyusb.h"
#elif __has_include("tinyusb.h")
    #include "tinyusb.h"
#else
    #include "tusb.h"
#endif
#include "tusb_msc_storage.h"

#include "telemetry_sd.h"
#include "driver/gpio.h"
#include "ui_kartbox.h" 

static const char *TAG = "USB_MSC";
static bool usb_active = false;

// Função auxiliar para UI segura
static void safe_ui_popup(const char *msg, uint32_t delay) {
    if (lvgl_port_lock(0)) {
        ui_show_popup(msg, delay);
        lvgl_port_unlock();
    }
}

void usb_msc_task(void *arg) {
    ESP_LOGI(TAG, ">>> ATIVANDO MODO USB <<<");

    // 1. GARANTIR QUE NENHUM ARQUIVO ESTÁ ABERTO
    // Não desmontamos o cartão, apenas paramos de usar.
    sd_stop_session(); 
    
    // Pequeno delay para garantir que o buffer do arquivo foi salvo (flush)
    vTaskDelay(pdMS_TO_TICKS(500));

    // 2. PEGAR O HANDLE EXISTENTE
    // Usamos o mesmo driver que já está funcionando no telemetry_sd.c
    sdmmc_card_t *card = sd_get_card_handle();
    
    if (card == NULL) {
        ESP_LOGE(TAG, "ERRO: O sistema não detectou o cartão no boot.");
        safe_ui_popup("ERRO: SD NAO INICIADO", 3000);
        vTaskDelete(NULL);
        return;
    }

    // 3. CONFIGURAR O ARMAZENAMENTO USB (MSC)
    ESP_LOGI(TAG, "Vinculando SD Card ao USB...");
    tinyusb_msc_sdmmc_config_t msc_cfg = {
        .card = card // Usa o ponteiro que já existe!
    };
    
    esp_err_t err = tinyusb_msc_storage_init_sdmmc(&msc_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Falha MSC Storage: %s", esp_err_to_name(err));
        safe_ui_popup("ERRO MSC INIT", 3000);
        vTaskDelete(NULL);
        return;
    }

    // 4. INSTALAR O DRIVER TINYUSB
    ESP_LOGI(TAG, "Iniciando Stack USB...");
    const tinyusb_config_t tusb_cfg = {
        .device_descriptor = NULL,
        .string_descriptor = NULL,
        .external_phy = false, // ESP32-P4 High Speed PHY
        .configuration_descriptor = NULL,
    };

    err = tinyusb_driver_install(&tusb_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Falha Driver USB: %s", esp_err_to_name(err));
        safe_ui_popup("ERRO DRIVER USB", 3000);
        vTaskDelete(NULL);
        return;
    }

    usb_active = true;
    ESP_LOGI(TAG, ">>> USB ATIVO COM SUCESSO <<<");
    
    safe_ui_popup("USB CONECTADO!\nCOPIE OS ARQUIVOS", 5000);

    // Aviso importante no log
    ESP_LOGW(TAG, "IMPORTANTE: Reinicie o ESP32 apos copiar os arquivos para evitar corrupcao.");

    vTaskDelete(NULL);
}

void usb_mode_start(void) {
    if (usb_active) {
        safe_ui_popup("JA ESTA ATIVO", 1000);
        return;
    }
    // Cria tarefa para não travar a interface
    xTaskCreate(usb_msc_task, "usb_task", 4096, NULL, 5, NULL);
}