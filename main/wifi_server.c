#include "wifi_server.h"
#include <string.h>
#include <sys/param.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "esp_vfs.h"
#include "dirent.h"

static const char *TAG = "WIFI_SRV";
static httpd_handle_t server = NULL;
static bool is_active = false;

// --- HANDLER: DOWNLOAD DE ARQUIVO ---
static esp_err_t download_get_handler(httpd_req_t *req) {
    char filepath[256];
    // Converte URL /files/nome.csv para /sdcard/nome.csv
    snprintf(filepath, sizeof(filepath), "/sdcard/%s", req->uri + 7); 

    FILE *fd = fopen(filepath, "r");
    if (!fd) {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/octet-stream"); // Força download
    httpd_resp_set_hdr(req, "Content-Disposition", "attachment");

    char *chunk = malloc(4096); // Buffer de 4KB
    if (!chunk) { fclose(fd); return ESP_FAIL; }

    size_t chunksize;
    do {
        chunksize = fread(chunk, 1, 4096, fd);
        if (chunksize > 0) {
            if (httpd_resp_send_chunk(req, chunk, chunksize) != ESP_OK) {
                fclose(fd); free(chunk); return ESP_FAIL;
            }
        }
    } while (chunksize != 0);

    fclose(fd);
    free(chunk);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

// --- HANDLER: LISTA DE ARQUIVOS (HTML) ---
static esp_err_t file_list_get_handler(httpd_req_t *req) {
    // CORREÇÃO: Usando a função padrão com HTTPD_RESP_USE_STRLEN
    httpd_resp_send_chunk(req, 
        "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
        "<style>body{font-family:sans-serif;padding:20px;background:#1a1a1a;color:white} "
        "a{display:block;padding:15px;margin:5px 0;background:#333;color:#00ff41;text-decoration:none;border-radius:5px;} "
        "a:hover{background:#444}</style></head><body><h2>🏎️ KartBox Logs</h2>", 
        HTTPD_RESP_USE_STRLEN);

    DIR *dir = opendir("/sdcard");
    struct dirent *entry;
    char line[512];

    if (dir) {
        while ((entry = readdir(dir)) != NULL) {
            // Filtra apenas CSV e LOG
            if (strstr(entry->d_name, ".csv") || strstr(entry->d_name, ".LOG") || strstr(entry->d_name, ".CSV")) {
                snprintf(line, sizeof(line), "<a href=\"/files/%s\">📄 %s</a>", entry->d_name, entry->d_name);
                httpd_resp_send_chunk(req, line, HTTPD_RESP_USE_STRLEN);
            }
        }
        closedir(dir);
    } else {
        httpd_resp_send_chunk(req, "<p>Erro ao ler SD Card</p>", HTTPD_RESP_USE_STRLEN);
    }

    httpd_resp_send_chunk(req, "</body></html>", HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

// --- INICIA HTTP SERVER ---
static void start_http_server(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 8192; // Pilha maior para arquivos
    config.max_uri_handlers = 8;

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t file_download = { .uri = "/files/*", .method = HTTP_GET, .handler = download_get_handler, .user_ctx = NULL };
        httpd_register_uri_handler(server, &file_download);

        httpd_uri_t file_list = { .uri = "/", .method = HTTP_GET, .handler = file_list_get_handler, .user_ctx = NULL };
        httpd_register_uri_handler(server, &file_list);
        ESP_LOGI(TAG, "Web Server Iniciado");
    }
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        ESP_LOGI(TAG, "Cliente conectado ao Wi-Fi");
    }
}

void wifi_server_start(void) {
    if (is_active) return;

    // Inicializa NVS se necessário
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    esp_netif_init();
    
    // Verifica se o loop já foi criado para evitar crash ao religar
    esp_err_t loop_err = esp_event_loop_create_default();
    if (loop_err != ESP_OK && loop_err != ESP_ERR_INVALID_STATE) {
        // Ignora erro se já estiver criado
    }

    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL);

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = "KartBox_Data",
            .ssid_len = strlen("KartBox_Data"),
            .password = "12345678",
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };

    if (strlen((char *)wifi_config.ap.password) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    esp_wifi_start();
    
    start_http_server();
    is_active = true;
}

void wifi_server_stop(void) {
    if (!is_active) return;
    if (server) { httpd_stop(server); server = NULL; }
    esp_wifi_stop();
    // Nota: Não chamamos esp_wifi_deinit() para poder religar rápido
    is_active = false;
}

bool wifi_server_is_active(void) {
    return is_active;
}