# 📘 Manual do Usuário - KartBox V1.0

Bem-vindo ao **KartBox**, seu sistema de telemetria avançada para Karting. Este manual explica como operar o painel, registrar seus tempos de volta e analisar seu desempenho.

---

## 🚦 Guia Rápido (Para quem tem pressa)

1. **Ligue o KartBox:** Conecte a bateria/alimentação.
2. **Aguarde o GPS:** Olhe para o topo direito da tela.
   - 🟡 **AMARELO (BUSCA...):** Aguarde, está procurando satélites.
   - 🟢 **VERDE (FIX):** Pronto! Pode entrar na pista.
3. **Marque a Pista:** Na sua volta de aquecimento, ao passar pela linha de chegada, aperte o botão **"MARCAR"**.
4. **Pilote:** O sistema agora marcará as voltas automaticamente.
5. **Finalizar:** Pare o Kart. Segure o botão **"RESET" por 2 segundos** para salvar os dados no cartão SD.

---

## 🖥️ Conhecendo a Interface

O sistema possui 3 abas principais na parte inferior da tela.

### 1. Aba RACE (Painel de Corrida)
É a tela principal que você usa enquanto pilota.

* **Velocidade (Grande):** Sua velocidade atual em KM/H.
* **Tempo (Centro):** Cronômetro da volta atual.
* **Delta (Abaixo):** A diferença de tempo para sua melhor volta.
    * 🟢 **Verde (Negativo):** Você está mais rápido que sua melhor volta.
    * 🔴 **Vermelho (Positivo):** Você está mais lento.
* **Informações de Volta:** Mostra o número da volta atual e o tempo da **Best Lap** (Melhor volta).
* **Status do GPS (Topo Direito):** Indica a qualidade do sinal (veja a seção *Diagnóstico de GPS*).

**Botões da Tela:**
1.  **MODO:** Alterna entre *Race* (Corrida) e *Qualy* (Classificação). (Apenas muda o visual/cor da borda para referência).
2.  **MARCAR:** Define o ponto GPS atual como a linha de chegada/largada. **Obrigatório fazer uma vez por sessão.**
3.  **RESET:**
    * *Toque Curto:* Zera o cronômetro (se não estiver gravando).
    * *Segurar 2s:* **Salva a sessão** no cartão SD e encerra a gravação.

### 2. Aba VOLTAS (Análise)
Use esta tela quando estiver nos boxes para ver como foi seu desempenho.

* **Menu de Seleção:** Escolha a corrida que deseja ver (ex: `SESSION_001.LOG` ou Data/Hora).
* **Botão Refresh (🔄):** Recarrega os dados do cartão SD e atualiza o gráfico.
* **Gráfico de Barras:** Mostra a velocidade média ou máxima de cada volta. O gráfico ajusta a escala automaticamente (se você andou a 90km/h, a barra sobe até lá).
* **Lista:** Lista detalhada com o tempo de cada volta.

### 3. Aba CFG (Configurações)
* **Status do SD:** Mostra quanto espaço livre existe no cartão de memória.
* **Apagar Tudo:** Botão vermelho para limpar o cartão SD (Cuidado: apaga todas as corridas!).

---

## 📡 Diagnóstico de GPS (Luzes de Status)

No canto superior direito da tela RACE, o texto muda de cor para te avisar sobre a saúde do sistema:

| Cor | Mensagem | O que significa? | O que fazer? |
| :--- | :--- | :--- | :--- |
| 🔴 **Vermelho** | **GPS: ERRO HW** | O painel não está recebendo dados do módulo GPS. | Verifique se o fio do GPS soltou ou se o módulo está desligado. |
| 🟡 **Amarelo** | **SATS: 0 BUSCA...** | O hardware está OK, mas sem sinal de satélite. | Leve o Kart para céu aberto. Saia de garagens ou coberturas. |
| 🟢 **Verde** | **SATS: 8 FIX** | Sistema travado com satélites. Precisão OK. | Tudo pronto para correr! |

---

## 💾 Salvando seus Dados

O KartBox possui um sistema de proteção para não perder dados se a bateria cair, mas o ideal é salvar manualmente.

1.  Ao entrar nos boxes, pare o Kart.
2.  Pressione o botão **RESET** e **mantenha pressionado**.
3.  Uma barra vermelha aparecerá escrito **"A SALVAR..."**.
4.  Quando a barra encher, solte o botão. Uma mensagem "SESSÃO SALVA" aparecerá.
5.  Agora é seguro desligar o aparelho.

---

## ❓ Perguntas Frequentes (FAQ)

**P: Esqueci de apertar "MARCAR" na primeira volta.**
R: O sistema não saberá onde a volta fecha. Seus tempos não serão registrados corretamente. Aperte MARCAR assim que passar pela linha de chegada na próxima oportunidade.

**P: O gráfico na aba VOLTAS está vazio.**
R: Selecione a corrida no menu e aperte o botão **Refresh (🔄)** ao lado dele.

**P: O GPS fica sempre em "ERRO HW" vermelho.**
R: Isso indica problema físico. Verifique o cabo que conecta o módulo GPS à placa principal (Fios TX/RX).

**P: Como passo os dados para o computador?**
R: Retire o cartão MicroSD, coloque no PC e abra os arquivos `.CSV` no Excel ou em softwares de análise de telemetria como o *RaceRender* ou *MoTeC i2* (requer conversão).

---
*KartBox Team - Desenvolvido para Vencer.*