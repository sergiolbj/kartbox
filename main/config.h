#ifndef CONFIG_H
#define CONFIG_H

#include "driver/gpio.h"
#include "driver/uart.h"

// ========== PINOS ESP32-P4 (JC4880P443C) ==========
// SD Card (Barramento SD1 Nativo)
#define SD_CMD_PIN          44  
#define SD_CLK_PIN          43  
#define SD_D0_PIN           39  
#define SD_PWR_EN_PIN       45  // P-MOSFET (0 = LIGADO)

// Botões Físicos
#define BTN_MODE_PIN        GPIO_NUM_33
#define BTN_SETLINE_PIN     GPIO_NUM_31
#define BTN_RESET_PIN       GPIO_NUM_30

// GPS (UART1)
#define GPS_UART_NUM        UART_NUM_1
#define GPS_TX_PIN          52
#define GPS_RX_PIN          51
#define GPS_BAUD_RATE       115200 

// MPU-6050 (I2C0)
#define MPU_I2C_NUM         I2C_NUM_0
#define MPU_SDA_PIN         7   
#define MPU_SCL_PIN         8   
#define MPU_I2C_FREQ        400000 // Frequência do barramento I2C

// ========== CONSTANTES DE TELEMETRIA ==========
#define MAX_LAPS            100    // Limite de voltas na memória
#define UI_UPDATE_MS        100 
#define GATE_RADIUS_M       12.0   // Raio do portão virtual (metros)
#define MIN_LAP_TIME_MS     20000  // Tempo mínimo de volta (evita triggers falsos)

#endif