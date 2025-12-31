import pandas as pd
import matplotlib.pyplot as plt
import glob
import os
import numpy as np
from fpdf import FPDF
from fpdf.enums import XPos, YPos
from datetime import datetime
import warnings

# Silenciar avisos de depreciação
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
        if ':' in str(t):
            m, s = str(t).strip().split(':')
            return int(m) * 60 + float(s)
        return float(t)
    except: return 0.0

def injetar_voltas(df_data, df_laps):
    if 'Lap' in df_data.columns: return df_data
    df_data['Lap'] = 1
    try:
        data_times = pd.to_datetime(df_data['Time'], format='%H:%M:%S', errors='coerce')
        for idx, row in df_laps.iterrows():
            lap_end = datetime.strptime(row['Time_of_day'], '%H:%M:%S')
            df_data.loc[data_times > lap_end, 'Lap'] = row['Lap'] + 1
    except: pass
    return df_data

def gerar_insights_ia(df_mov, df_laps):
    insights = []
    if not df_laps.empty and len(df_laps) > 2:
        std_dev = df_laps['Time_sec'].std()
        if std_dev < 0.8: insights.append(f"- CONSISTÊNCIA: Performance de elite (Variação de {std_dev:.2f}s).")
        elif std_dev > 2.0: insights.append(f"- RITMO: Oscilação detectada ({std_dev:.2f}s). Foque na repetibilidade.")
    return insights

def processar_sessao():
    arquivos_data = glob.glob("data_*.csv")
    for f_data in arquivos_data:
        sessao_id = f_data.replace("data_", "").replace(".csv", "")
        base_dir = f"Analise_{sessao_id}"
        os.makedirs(os.path.join(base_dir, "Voltas"), exist_ok=True)
        
        try:
            df = pd.read_csv(f_data)
            df.columns = [c.strip().capitalize() for c in df.columns]
            f_laps = f_data.replace('data_', 'laps_')
            df_laps = pd.DataFrame()
            setores = {'S1': 999.0, 'S2': 999.0, 'S3': 999.0}
            stats_modo = {}

            if os.path.exists(f_laps):
                df_laps = pd.read_csv(f_laps)
                df_laps.columns = [c.strip().capitalize() for c in df_laps.columns]
                df_laps['Time_sec'] = df_laps['Time'].apply(converter_tempo_sec)
                df = injetar_voltas(df, df_laps)

            df_mov = df[df['Speed'] > 5].copy()

            if 'Lap' in df_mov.columns:
                for v in df_mov['Lap'].unique():
                    lap_data = df_mov[df_mov['Lap'] == v]
                    if len(lap_data) < 10: continue
                    chunks = np.array_split(lap_data, 3)
                    for i, chunk in enumerate(chunks):
                        s_time = (chunk['Timestamp_ms'].max() - chunk['Timestamp_ms'].min()) / 1000.0
                        if 0 < s_time < setores[f'S{i+1}']: setores[f'S{i+1}'] = s_time

            # --- GRÁFICOS ---
            img_evol = os.path.join(base_dir, "evolucao.png")
            plt.figure(figsize=(10, 4))
            if not df_laps.empty:
                for m, c in [('QUALY', COLOR_QUALY), ('RACE', COLOR_RACE)]:
                    m_df = df_laps[df_laps['Mode'] == m]
                    if not m_df.empty: plt.plot(m_df['Lap'], m_df['Time_sec'], marker='o', label=f'Modo {m}', color=c)
            plt.title('Evolução de Performance'); plt.legend(); plt.grid(True, alpha=0.3); plt.savefig(img_evol, dpi=120); plt.close()

            # --- PDF ---
            pdf = KartReport()
            pdf.set_margins(15, 15, 15); pdf.add_page()
            
            # 1. Ideal Lap (Estilo Simplificado)
            pdf.set_fill_color(240, 240, 240); pdf.set_font('helvetica', 'B', 12)
            pdf.cell(0, 10, f" Sessão: {sessao_id}", fill=True, new_x=XPos.LMARGIN, new_y=YPos.NEXT)
            pdf.ln(2); pdf.set_font('helvetica', '', 10)
            ideal_t = sum(setores.values())
            pdf.cell(pdf.epw/4, 10, f"S1: {setores['S1']:.3f}s", border=1, align='C')
            pdf.cell(pdf.epw/4, 10, f"S2: {setores['S2']:.3f}s", border=1, align='C')
            pdf.cell(pdf.epw/4, 10, f"S3: {setores['S3']:.3f}s", border=1, align='C')
            pdf.set_font('helvetica', 'B', 10); pdf.cell(pdf.epw/4, 10, f"IDEAL: {ideal_t:.3f}s", border=1, align='C', new_x=XPos.LMARGIN, new_y=YPos.NEXT)
            
            # 2. Insights IA
            pdf.ln(5); pdf.set_text_color(192, 57, 43); pdf.set_font('helvetica', 'B', 11)
            pdf.cell(0, 8, "ENGINEER INSIGHTS:", new_x=XPos.LMARGIN, new_y=YPos.NEXT)
            pdf.set_text_color(0, 0, 0); pdf.set_font('helvetica', '', 10)
            for h in gerar_insights_ia(df_mov, df_laps):
                pdf.multi_cell(pdf.epw - 10, 6, h, new_x=XPos.LMARGIN, new_y=YPos.NEXT)

            pdf.ln(5); pdf.image(img_evol, x=15, y=pdf.get_y(), w=180); pdf.set_y(pdf.get_y() + 85)

            # 3. Análise Detalhada por Modo (Páginas Separadas, Y Iniciando em 60)
            for modo_atual in ['QUALY', 'RACE']:
                laps_in_mode = df_mov[df_mov['Mode'] == modo_atual]['Lap'].unique()
                if len(laps_in_mode) == 0: continue
                
                pdf.add_page(); pdf.set_font('helvetica', 'B', 16)
                pdf.cell(0, 20, f"ANÁLISE DETALHADA: MODO {modo_atual}", align='C', new_x=XPos.LMARGIN, new_y=YPos.NEXT)
                
                for i, v_num in enumerate(sorted(laps_in_mode)):
                    lap_data = df_mov[df_mov['Lap'] == v_num]
                    if len(lap_data) < 5: continue
                    
                    plt.figure(figsize=(4,4)); plt.scatter(lap_data['Lon'], lap_data['Lat'], c=lap_data['Speed'], cmap='RdYlGn', s=12)
                    plt.axis('off'); plt.gca().set_aspect('equal')
                    img_p = os.path.join(base_dir, "Voltas", f"V{int(v_num)}.png"); plt.savefig(img_p, bbox_inches='tight', pad_inches=0.1); plt.close()
                    
                    x_pos = 15 if (i % 2 == 0) else 110
                    # Começar as imagens em y=60 para não cobrir o título da página
                    y_offset = 60 + (((i % 4) // 2) * 110)
                    
                    if i > 0 and i % 4 == 0:
                        pdf.add_page(); y_offset = 60
                    
                    tempo_v = "N/A"
                    if not df_laps.empty:
                        row_v = df_laps[(df_laps['Mode'] == modo_atual) & (df_laps['Lap'] == v_num)]
                        if not row_v.empty: tempo_v = f"{row_v['Time'].iloc[0]}s"

                    pdf.image(img_p, x=x_pos, y=y_offset, w=85)
                    pdf.set_xy(x_pos, y_offset + 88); pdf.set_font('helvetica', 'B', 10)
                    pdf.cell(85, 8, f"VOLTA {int(v_num)} - {tempo_v}", align='C')

            # 4. Ranking Final (Última Página)
            if not df_laps.empty:
                pdf.add_page(); pdf.set_font('helvetica', 'B', 16)
                pdf.cell(0, 20, "RANKING FINAL DA SESSÃO", align='C', new_x=XPos.LMARGIN, new_y=YPos.NEXT)
                pdf.ln(10)
                
                top3 = df_laps.sort_values(by='Time_sec').head(3)
                col_w = pdf.epw / 4
                pdf.set_fill_color(230, 230, 230); pdf.set_font('helvetica', 'B', 12)
                for h in ["RANK", "VOLTA", "TEMPO", "MODO"]: pdf.cell(col_w, 12, h, border=1, align='C', fill=True)
                pdf.ln()
                
                pdf.set_font('helvetica', '', 12)
                for i, (idx, row) in enumerate(top3.iterrows(), 1):
                    pdf.cell(col_w, 12, f"{i} Lugar", border=1, align='C')
                    pdf.cell(col_w, 12, str(int(row['Lap'])), border=1, align='C')
                    pdf.cell(col_w, 12, f"{row['Time']}s", border=1, align='C')
                    pdf.cell(col_w, 12, str(row['Mode']), border=1, align='C', new_x=XPos.LMARGIN, new_y=YPos.NEXT)

            pdf.output(os.path.join(base_dir, f"Relatorio_Final_v17_{sessao_id}.pdf"))
            print(f">>> Relatório v17 gerado com sucesso.")
        except Exception as e: print(f"Erro: {e}")

if __name__ == "__main__":
    processar_sessao()