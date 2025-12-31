#ifndef WIFI_SERVER_H
#define WIFI_SERVER_H

#include <stdbool.h>

// Inicia o Wi-Fi (SoftAP) e o Servidor Web
void wifi_server_start(void);

// Para o Wi-Fi para economizar bateria/CPU
void wifi_server_stop(void);

// Retorna se o Wi-Fi está ativo
bool wifi_server_is_active(void);

#endif