# 🏎️ KartBox - Telemetria Avançada para Karting (ESP32)

O **KartBox** é um sistema de telemetria e dashboard para Karts de competição, desenvolvido com **ESP32** (ESP-IDF) e **LVGL v9**. Ele oferece monitoramento em tempo real de tempos de volta, velocidade, delta (gap) e registro de dados em cartão SD para análise posterior.

![Status](https://img.shields.io/badge/Status-Stable-brightgreen)
![Platform](https://img.shields.io/badge/Platform-ESP32--P4-blue)
![Framework](https://img.shields.io/badge/Framework-ESP--IDF-red)
![Interface](https://img.shields.io/badge/GUI-LVGL_v9-orange)

## 📸 Screenshots

*(Espaço reservado para você colocar uma foto ou print da tela do seu projeto)*

## 🚀 Funcionalidades Principais

### 🏁 Dashboard de Corrida (Aba Race)
- **Velocidade em Tempo Real:** Leitura de GPS de alta precisão.
- **Lap Timer:** Tempo da volta atual, última volta e **Best Lap**.
- **Live Delta:** Mostra a diferença de tempo para a melhor volta em tempo real (Verde = Mais rápido, Vermelho = Mais lento).
- **Modos de Corrida:** Alternância entre *Qualy* (Classificação) e *Race* (Corrida).

### 📊 Análise de Dados (Aba Voltas)
- **Histórico de Sessões:** Lista corridas salvas no SD pelo nome do arquivo (ex: `SESSION_001.LOG`).
- **Gráfico Dinâmico:**
  - Eixo Y (Velocidade) escala automaticamente conforme a maior velocidade atingida.
  - Eixo X exibe o número das voltas.
- **Botão Refresh:** Recarrega os dados do cartão SD sem reiniciar o sistema.

### 💾 Datalogger Robusto (SD Card)
- **Arquitetura Anti-Crash:** O salvamento de arquivos pesados roda em uma **Task FreeRTOS dedicada**, isolada da interface gráfica (UI), prevenindo erros de *Spinlock* e travamentos visuais.
- **CSV Format:** Dados exportáveis (Lat, Lon, Speed, Timestamp) compatíveis com softwares de análise.
- **Detecção Inteligente:** Identifica arquivos automaticamente na inicialização.

### 🛰️ Monitoramento de Saúde do GPS
Diagnóstico visual avançado no topo da tela:
- 🔴 **GPS: ERRO HW:** Módulo desconectado ou falha de comunicação (RX/TX).
- 🟡 **SATS: 0 BUSCA...:** Hardware OK, aguardando sinal de satélites.
- 🟢 **SATS: X FIX:** Sinal travado e pronto para uso.

## 🛠️ Hardware Utilizado

- **MCU:** ESP32-P4 (Function EV Board) ou compatível (ESP32-S3/WROOM).
- **Display:** LCD com interface RGB/SPI (Driver ST7701/EK79007).
- **GPS:** Módulo NMEA (ex: BN-880, NEO-6M) via UART.
- **Armazenamento:** Módulo MicroSD (SDMMC ou SPI).
- **IMU:** (Opcional) MPU6050/9250 para dados de força G.

## 📂 Estrutura do Projeto

```text
/main
├── main.c              # Ponto de entrada e setup do hardware
├── ui_kartbox.c        # Lógica da Interface (LVGL), Gráficos e Eventos
├── telemetry_gps.c     # Parser NMEA e lógica de Delta
├── telemetry_sd.c      # Gerenciamento de Arquivos e Logs
├── telemetry_mpu.c     # Leitura de sensores inerciais
└── ...

🎮 Como Usar
Inicialização: Ao ligar, aguarde o status do GPS ficar VERDE (FIX).

Marcar Linha de Chegada: Na primeira volta, pressione "MARCAR" onde deseja definir a linha de chegada/largada.

Correr: O sistema detectará as voltas automaticamente via coordenadas GPS.

Salvar: Ao fim da corrida, segure o botão "RESET" por 2 segundos.

Nota: O salvamento ocorre em background. Uma barra de progresso indicará a conclusão.

Revisar: Vá até a aba "VOLTAS", selecione a corrida e clique no botão de Refresh 🔄 para ver o gráfico de desempenho.

🤝 Contribuição
Contribuições são bem-vindas! Sinta-se à vontade para abrir Issues ou enviar Pull Requests.

📜 Licença
Este projeto está sob a licença MIT. Sinta-se livre para usar e modificar.