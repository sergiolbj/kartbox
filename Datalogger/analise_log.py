import pandas as pd
import matplotlib.pyplot as plt
import glob
import os
import numpy as np
from fpdf import FPDF
from fpdf.enums import XPos, YPos
from datetime import datetime
import warnings

# Silenciar avisos de depreciação do NumPy/Pandas
warnings.filterwarnings("ignore", category=FutureWarning)

plt.style.use('default') 
COLOR_QUALY = '#E67E22' 
COLOR_RACE = '#27AE60'  

class KartReport(FPDF):
    def header(self):
        self.set_font('helvetica', 'B', 20)
        self.set_text_color(44, 62, 80)
        self.cell(0, 15, 'KARTBOX - AI TRACK INSIGHTS PRO', align='L', new_x=XPos.RIGHT, new_y=YPos.TOP)
        self.set_font('helvetica', 'I', 9)
        self.set_text_color(127, 140, 141)
        self.cell(0, 15, f'Relatório Técnico | {datetime.now().strftime("%d/%m/%Y")}', align='R', new_x=XPos.LMARGIN, new_y=YPos.NEXT)
        self.line(15, 28, 195, 28)
        self.ln(10)

    def footer(self):
        self.set_y(-15)
        self.set_font('helvetica', 'I', 8)
        self.set_text_color(149, 165, 166)
        self.cell(0, 10, f'KartBox AI System - Página {self.page_no()}', align='C')

def converter_tempo_sec(t):
    try:
        t_str = str(t).strip()
        if ':' in t_str:
            parts = t_str.split(':')
            if len(parts) == 2:
                m, s = parts
                return int(m) * 60 + float(s)
            elif len(parts) == 3:
                h, m, s = parts
                return int(h) * 3600 + int(m) * 60 + float(s)
        return float(t_str)
    except: return 0.0

def processar_sessao():
    arquivos_data = glob.glob("data_*.csv")
    if not arquivos_data:
        print("Nenhum arquivo data_*.csv encontrado!")
        return

    for f_data in arquivos_data:
        sessao_id = f_data.replace("data_", "").replace(".csv", "")
        base_dir = f"Analise_{sessao_id}"
        os.makedirs(os.path.join(base_dir, "Voltas"), exist_ok=True)
        
        try:
            # 1. Leitura dos Dados (Novo Padrão)
            df = pd.read_csv(f_data)
            df.columns = [c.strip() for c in df.columns]
            
            # Normalização do Modo (0/1 -> Texto)
            if df['Mode'].dtype in [np.int64, np.float64]:
                df['Mode'] = df['Mode'].map({0: 'QUALY', 1: 'RACE'}).fillna('RACE')
            else:
                df['Mode'] = df['Mode'].astype(str).str.upper()

            # 2. Leitura das Voltas (Novo Padrão com Header)
            f_laps = f_data.replace('data_', 'laps_')
            df_laps = pd.DataFrame()
            if os.path.exists(f_laps):
                df_laps = pd.read_csv(f_laps)
                df_laps.columns = [c.strip() for c in df_laps.columns]
                df_laps['Time_sec'] = df_laps['Time'].apply(converter_tempo_sec)
                
                if df_laps['Mode'].dtype in [np.int64, np.float64]:
                    df_laps['Mode'] = df_laps['Mode'].map({0: 'QUALY', 1: 'RACE'}).fillna('RACE')
                else:
                    df_laps['Mode'] = df_laps['Mode'].astype(str).str.upper()

            df_mov = df[df['Speed'] > 5].copy()
            setores = {'S1': 999.0, 'S2': 999.0, 'S3': 999.0}

            # --- CÁLCULOS DE SETORES ---
            for v in df_mov['Lap'].unique():
                if v == 0: continue
                lap_data = df_mov[df_mov['Lap'] == v]
                if len(lap_data) < 10: continue
                chunks = np.array_split(lap_data, 3)
                for i, chunk in enumerate(chunks):
                    s_name = f'S{i+1}'
                    s_time = (chunk['Timestamp_ms'].max() - chunk['Timestamp_ms'].min()) / 1000.0
                    if 0 < s_time < setores[s_name]: setores[s_name] = s_time

            # --- GERAÇÃO DE GRÁFICOS ---
            img_evol = os.path.join(base_dir, "evolucao.png")
            plt.figure(figsize=(10, 4))
            has_plot = False
            for m, c in [('QUALY', COLOR_QUALY), ('RACE', COLOR_RACE)]:
                m_df = df_laps[df_laps['Mode'] == m] if not df_laps.empty else pd.DataFrame()
                if not m_df.empty: 
                    plt.plot(m_df['Lap'], m_df['Time_sec'], marker='o', label=f'Modo {m}', color=c)
                    has_plot = True
            if has_plot:
                plt.title('Evolução de Performance'); plt.legend(); plt.grid(True, alpha=0.3)
                plt.ylabel('Tempo (s)'); plt.xlabel('Volta')
                plt.savefig(img_evol, dpi=120)
            plt.close()

            # --- PDF CONSTRUTOR ---
            pdf = KartReport()
            pdf.set_margins(15, 15, 15); pdf.add_page()
            
            # Ideal Lap
            pdf.set_fill_color(240, 240, 240); pdf.set_font('helvetica', 'B', 12)
            pdf.cell(0, 10, f" Sessão: {sessao_id}", fill=True, new_x=XPos.LMARGIN, new_y=YPos.NEXT)
            pdf.ln(2); pdf.set_font('helvetica', '', 10)
            ideal_t = sum(setores.values())
            col_w = pdf.epw / 4
            pdf.cell(col_w, 10, f"S1: {setores['S1']:.3f}s", border=1, align='C')
            pdf.cell(col_w, 10, f"S2: {setores['S2']:.3f}s", border=1, align='C')
            pdf.cell(col_w, 10, f"S3: {setores['S3']:.3f}s", border=1, align='C')
            pdf.set_font('helvetica', 'B', 10); pdf.cell(col_w, 10, f"IDEAL: {ideal_t:.3f}s", border=1, align='C', new_x=XPos.LMARGIN, new_y=YPos.NEXT)
            
            # Insights IA
            pdf.ln(5); pdf.set_text_color(192, 57, 43); pdf.set_font('helvetica', 'B', 11)
            pdf.cell(0, 8, "ENGINEER INSIGHTS:", new_x=XPos.LMARGIN, new_y=YPos.NEXT)
            pdf.set_text_color(0, 0, 0); pdf.set_font('helvetica', '', 10)
            if not df_laps.empty and len(df_laps) > 2:
                std_dev = df_laps['Time_sec'].std()
                if std_dev < 0.8: pdf.multi_cell(0, 6, f"- CONSISTÊNCIA: Performance de elite (Variação de {std_dev:.2f}s).")
                elif std_dev > 2.0: pdf.multi_cell(0, 6, f"- RITMO: Oscilação detectada ({std_dev:.2f}s). Foque na repetibilidade.")

            if os.path.exists(img_evol):
                pdf.ln(5); pdf.image(img_evol, x=15, w=180); pdf.set_y(pdf.get_y() + 85)

            # Análise Detalhada por Modo (Páginas Separadas)
            for modo_atual in ['QUALY', 'RACE']:
                laps_in_mode = sorted([v for v in df_mov[df_mov['Mode'] == modo_atual]['Lap'].unique() if v > 0])
                if not laps_in_mode: continue
                
                pdf.add_page(); pdf.set_font('helvetica', 'B', 16)
                pdf.cell(0, 20, f"ANÁLISE DETALHADA: MODO {modo_atual}", align='C', new_x=XPos.LMARGIN, new_y=YPos.NEXT)
                
                for i, v_num in enumerate(laps_in_mode):
                    lap_data = df_mov[df_mov['Lap'] == v_num]
                    plt.figure(figsize=(4,4)); plt.scatter(lap_data['Lon'], lap_data['Lat'], c=lap_data['Speed'], cmap='RdYlGn', s=12)
                    plt.axis('off'); plt.gca().set_aspect('equal')
                    img_p = os.path.join(base_dir, "Voltas", f"V{int(v_num)}.png"); plt.savefig(img_p, bbox_inches='tight', pad_inches=0.1); plt.close()
                    
                    x_pos = 15 if (i % 2 == 0) else 110
                    y_offset = 60 + (((i % 4) // 2) * 110)
                    if i > 0 and i % 4 == 0: pdf.add_page(); y_offset = 60
                    
                    tempo_v = "N/A"
                    if not df_laps.empty:
                        row_v = df_laps[df_laps['Lap'] == v_num]
                        if not row_v.empty: tempo_v = row_v['Time'].iloc[0]

                    pdf.image(img_p, x=x_pos, y=y_offset, w=85)
                    pdf.set_xy(x_pos, y_offset + 88); pdf.set_font('helvetica', 'B', 10)
                    pdf.cell(85, 8, f"VOLTA {int(v_num)} - {tempo_v}s", align='C')

            # Ranking Final
            if not df_laps.empty:
                pdf.add_page(); pdf.set_font('helvetica', 'B', 16)
                pdf.cell(0, 20, "RANKING FINAL DA SESSÃO", align='C', new_x=XPos.LMARGIN, new_y=YPos.NEXT)
                pdf.ln(10)
                top3 = df_laps.sort_values(by='Time_sec').head(3)
                col_w = pdf.epw / 4
                pdf.set_fill_color(230, 230, 230); pdf.set_font('helvetica', 'B', 12)
                for h in ["RANK", "VOLTA", "TEMPO", "MODO"]: pdf.cell(col_w, 12, h, border=1, align='C', fill=True)
                pdf.ln(); pdf.set_font('helvetica', '', 12)
                for i, (idx, row) in enumerate(top3.iterrows(), 1):
                    pdf.cell(col_w, 12, f"{i} Lugar", border=1, align='C')
                    pdf.cell(col_w, 12, str(int(row['Lap'])), border=1, align='C')
                    pdf.cell(col_w, 12, f"{row['Time']}s", border=1, align='C')
                    pdf.cell(col_w, 12, str(row['Mode']), border=1, align='C', new_x=XPos.LMARGIN, new_y=YPos.NEXT)

            pdf.output(os.path.join(base_dir, f"Relatorio_Final_v19_{sessao_id}.pdf"))
            print(f">>> Relatório v19 gerado com sucesso.")
        except Exception as e:
            print(f"Erro no processamento: {e}")
            import traceback; traceback.print_exc()

if __name__ == "__main__":
    processar_sessao()