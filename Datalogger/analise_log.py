import pandas as pd
import matplotlib.pyplot as plt
import glob
import os
import numpy as np
from fpdf import FPDF
from fpdf.enums import XPos, YPos
from datetime import datetime
from scipy.interpolate import interp1d
import warnings

warnings.filterwarnings("ignore", category=FutureWarning)

plt.style.use('default') 
COLOR_QUALY = '#E67E22' 
COLOR_RACE = '#27AE60'  
COLOR_DELTA = '#2980B9'

class KartReport(FPDF):
    def header(self):
        self.set_font('helvetica', 'B', 20)
        self.set_text_color(44, 62, 80)
        self.cell(0, 15, 'KARTBOX - TRACK INSIGHTS ULTIMATE', align='L', new_x=XPos.RIGHT, new_y=YPos.TOP)
        self.set_font('helvetica', 'I', 9)
        self.set_text_color(127, 140, 141)
        self.cell(0, 15, f'Relatório Pro | {datetime.now().strftime("%d/%m/%Y")}', align='R', new_x=XPos.LMARGIN, new_y=YPos.NEXT)
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
            if len(parts) == 2: return int(parts[0]) * 60 + float(parts[1])
            if len(parts) == 3: return int(parts[0]) * 3600 + int(parts[1]) * 60 + float(parts[2])
        return float(t_str)
    except: return 0.0

def calcular_distancia_acumulada(lap_df):
    coords = lap_df[['Lat', 'Lon']].values
    diffs = np.diff(coords, axis=0)
    dists = np.sqrt(np.sum(diffs**2, axis=1))
    return np.concatenate(([0], np.cumsum(dists)))

def processar_sessao():
    arquivos_data = glob.glob("data_*.csv")
    for f_data in arquivos_data:
        sessao_id = f_data.replace("data_", "").replace(".csv", "")
        base_dir = f"Analise_{sessao_id}"
        os.makedirs(os.path.join(base_dir, "Voltas"), exist_ok=True)
        
        try:
            # 1. Carga de Dados conforme firmware
            df = pd.read_csv(f_data)
            df.columns = [c.strip() for c in df.columns]
            f_laps = f_data.replace('data_', 'laps_')
            df_laps = pd.read_csv(f_laps)
            df_laps.columns = [c.strip() for c in df_laps.columns]
            df_laps['Time_sec'] = df_laps['Time'].apply(converter_tempo_sec)
            
            # 2. Melhor Volta para Referência de Delta
            best_lap_num = df_laps.sort_values('Time_sec')['Lap'].iloc[0]
            df_mov = df[df['Speed'] > 5].copy()
            ref_data = df_mov[df_mov['Lap'] == best_lap_num].copy()
            ref_data['Dist'] = calcular_distancia_acumulada(ref_data)
            ref_data['Elapsed'] = (ref_data['Timestamp_ms'] - ref_data['Timestamp_ms'].min()) / 1000.0
            ref_interp = interp1d(ref_data['Dist'], ref_data['Elapsed'], bounds_error=False, fill_value="extrapolate")

            # 3. Cálculo de Setores (Ideal Lap)
            setores = {'S1': 999.0, 'S2': 999.0, 'S3': 999.0}
            for v in df_mov['Lap'].unique():
                if v == 0: continue
                lap_pts = df_mov[df_mov['Lap'] == v]
                if len(lap_pts) < 15: continue
                chunks = np.array_split(lap_pts, 3)
                for i, chunk in enumerate(chunks):
                    s_t = (chunk['Timestamp_ms'].max() - chunk['Timestamp_ms'].min()) / 1000.0
                    if 0 < s_t < setores[f'S{i+1}']: setores[f'S{i+1}'] = s_t

            # --- PDF CONSTRUTOR ---
            pdf = KartReport()
            pdf.set_margins(15, 15, 15); pdf.add_page()
            
            # Ideal Lap
            pdf.set_fill_color(240, 240, 240); pdf.set_font('helvetica', 'B', 12)
            pdf.cell(0, 10, f" Sessão: {sessao_id} | Ref. Delta: Volta {best_lap_num}", fill=True, new_x=XPos.LMARGIN, new_y=YPos.NEXT)
            pdf.ln(2); pdf.set_font('helvetica', '', 10)
            ideal_t = sum(setores.values())
            pdf.cell(pdf.epw/4, 10, f"S1: {setores['S1']:.3f}s", border=1, align='C')
            pdf.cell(pdf.epw/4, 10, f"S2: {setores['S2']:.3f}s", border=1, align='C')
            pdf.cell(pdf.epw/4, 10, f"S3: {setores['S3']:.3f}s", border=1, align='C')
            pdf.set_font('helvetica', 'B', 10); pdf.cell(pdf.epw/4, 10, f"IDEAL: {ideal_t:.3f}s", border=1, align='C', new_x=XPos.LMARGIN, new_y=YPos.NEXT)

            # Análise por Modo (Qualy/Race)
            for modo in ['QUALY', 'RACE']:
                laps = sorted([v for v in df_mov[df_mov['Mode'] == modo]['Lap'].unique() if v > 0])
                if not laps: continue
                
                pdf.add_page(); pdf.set_font('helvetica', 'B', 16)
                pdf.cell(0, 15, f"ANÁLISE E DELTA: MODO {modo}", align='C', new_x=XPos.LMARGIN, new_y=YPos.NEXT)
                
                for i, v_num in enumerate(laps):
                    lap_data = df_mov[df_mov['Lap'] == v_num].copy()
                    lap_data['Dist'] = calcular_distancia_acumulada(lap_data)
                    lap_data['Elapsed'] = (lap_data['Timestamp_ms'] - lap_data['Timestamp_ms'].min()) / 1000.0
                    
                    dist_grid = np.linspace(0, max(lap_data['Dist'].max(), ref_data['Dist'].max()), 100)
                    delta = interp1d(lap_data['Dist'], lap_data['Elapsed'], fill_value="extrapolate")(dist_grid) - ref_interp(dist_grid)

                    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(6, 8), gridspec_kw={'height_ratios': [2, 1]})
                    ax1.scatter(lap_data['Lon'], lap_data['Lat'], c=lap_data['Speed'], cmap='RdYlGn', s=15)
                    ax1.set_aspect('equal'); ax1.axis('off')
                    ax2.plot(dist_grid, delta, color=COLOR_DELTA); ax2.axhline(0, color='black', alpha=0.5)
                    ax2.fill_between(dist_grid, delta, 0, where=(delta > 0), color='red', alpha=0.2)
                    ax2.fill_between(dist_grid, delta, 0, where=(delta < 0), color='green', alpha=0.2)
                    
                    img_path = os.path.join(base_dir, "Voltas", f"V{v_num}_D.png")
                    plt.tight_layout(); plt.savefig(img_path, dpi=100); plt.close()
                    
                    y_img = 60 + (((i % 2) // 2) * 110) # 1 volta por página com delta para clareza
                    if i > 0: pdf.add_page()
                    
                    pdf.image(img_path, x=25, y=50, w=160)
                    t_v = df_laps[df_laps['Lap'] == v_num]['Time'].iloc[0]
                    pdf.set_xy(25, 200); pdf.set_font('helvetica', 'B', 12)
                    pdf.cell(160, 10, f"VOLTA {int(v_num)} - {t_v}s (Delta vs Recorde)", align='C')

            # Ranking Final
            pdf.add_page(); pdf.set_font('helvetica', 'B', 16)
            pdf.cell(0, 20, "RANKING FINAL DA SESSÃO", align='C', new_x=XPos.LMARGIN, new_y=YPos.NEXT)
            top3 = df_laps.sort_values('Time_sec').head(3)
            col_w = pdf.epw / 4
            pdf.set_fill_color(230, 230, 230); pdf.set_font('helvetica', 'B', 12)
            for h in ["RANK", "VOLTA", "TEMPO", "MODO"]: pdf.cell(col_w, 12, h, border=1, align='C', fill=True)
            pdf.ln()
            for i, (idx, row) in enumerate(top3.iterrows(), 1):
                pdf.cell(col_w, 12, f"{i} Lugar", border=1, align='C')
                pdf.cell(col_w, 12, str(int(row['Lap'])), border=1, align='C')
                pdf.cell(col_w, 12, f"{row['Time']}s", border=1, align='C')
                pdf.cell(col_w, 12, str(row['Mode']), border=1, align='C', new_x=XPos.LMARGIN, new_y=YPos.NEXT)

            pdf.output(os.path.join(base_dir, f"Relatorio_Full_v21_{sessao_id}.pdf"))
            print(f">>> Relatório v21 gerado com sucesso.")
        except Exception as e:
            print(f"Erro: {e}"); import traceback; traceback.print_exc()

if __name__ == "__main__":
    processar_sessao()