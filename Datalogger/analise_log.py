import pandas as pd
import matplotlib.pyplot as plt
import glob
import os
import numpy as np
from fpdf import FPDF
from fpdf.enums import XPos, YPos
from datetime import datetime
from scipy.interpolate import interp1d
from scipy.signal import find_peaks, savgol_filter
import warnings

# =================================================================
# PAINEL DE CONTROLE - CONFIGURAÇÕES VALIDADAS POR VOCÊ
# =================================================================
CURVE_THRESHOLD = 0.1      
CURVE_MIN_DISTANCE = 30     
CURVE_WINDOW_PERIODS = 20   
CURVE_SMOOTHING = 25         
CLUSTER_METERS_GAP = 7.5     
# =================================================================

warnings.filterwarnings("ignore")
np.seterr(divide='ignore', invalid='ignore') 
plt.style.use('default') 
COLOR_DELTA = '#2980B9'
COLOR_REF = '#95a5a6' 
COLOR_LAP = '#27ae60' 

class KartReport(FPDF):
    def header(self):
        self.set_font('helvetica', 'B', 20)
        self.set_text_color(44, 62, 80)
        self.cell(0, 15, 'KARTBOX - RACE DIRECTOR PRO', align='L', border=0, new_x=XPos.RIGHT, new_y=YPos.TOP)
        self.set_font('helvetica', 'I', 9)
        self.set_text_color(127, 140, 141)
        self.cell(0, 15, f'Advanced Telemetry v4.8 | {datetime.now().strftime("%d/%m/%Y")}', align='R', new_x=XPos.LMARGIN, new_y=YPos.NEXT)
        self.line(15, 28, 195, 28)
        self.ln(10)

    def footer(self):
        self.set_y(-15)
        self.set_font('helvetica', 'I', 8)
        self.set_text_color(149, 165, 166)
        self.cell(0, 10, f'Análise Técnica KartBox - Página {self.page_no()}', align='C')

def converter_tempo_sec(t):
    try:
        t_str = str(t).strip()
        if ':' in t_str:
            parts = t_str.split(':')
            if len(parts) == 2: return int(parts[0]) * 60 + float(parts[1])
            if len(parts) == 3: return int(parts[0]) * 3600 + int(parts[1]) * 60 + float(parts[2])
        return float(t_str)
    except: return 0.0

def calcular_distancia_metros(df):
    coords = df[['Lat', 'Lon']].values
    if len(coords) < 2: return np.zeros(len(df))
    diffs = np.diff(coords, axis=0)
    dists_coord = np.sqrt(np.sum(diffs**2, axis=1))
    return np.concatenate(([0], np.cumsum(dists_coord * 111111)))

def gerar_mapa_master(df_mov, top_laps):
    todas_curvas = []
    for v_num in top_laps:
        lap_data = df_mov[df_mov['Lap'] == v_num].copy().reset_index(drop=True)
        dy = np.diff(lap_data['Lat']); dx = np.diff(lap_data['Lon'])
        headings = np.unwrap(np.arctan2(dy, dx))
        curvatura = np.abs(pd.Series(headings).diff(periods=CURVE_WINDOW_PERIODS)).fillna(0).values
        curvatura_smooth = savgol_filter(curvatura, CURVE_SMOOTHING, 3) if len(curvatura) > CURVE_SMOOTHING else curvatura
        peaks, _ = find_peaks(curvatura_smooth, height=CURVE_THRESHOLD, distance=CURVE_MIN_DISTANCE)
        for p in peaks:
            todas_curvas.append({'dist': lap_data.loc[p, 'Dist'], 'lat': lap_data.loc[p, 'Lat'], 'lon': lap_data.loc[p, 'Lon']})
    
    if not todas_curvas: return []
    curvas_finais = []; todas_curvas = sorted(todas_curvas, key=lambda x: x['dist'])
    atual_grupo = [todas_curvas[0]]
    for i in range(1, len(todas_curvas)):
        if todas_curvas[i]['dist'] - atual_grupo[-1]['dist'] < CLUSTER_METERS_GAP:
            atual_grupo.append(todas_curvas[i])
        else:
            curvas_finais.append(atual_grupo); atual_grupo = [todas_curvas[i]]
    curvas_finais.append(atual_grupo)
    
    return [{'id': i+1, 'dist_ref': np.mean([c['dist'] for c in g]), 'lat': np.mean([c['lat'] for c in g]), 'lon': np.mean([c['lon'] for c in g])} for i, g in enumerate(curvas_finais)]

def processar_sessao():
    arquivos_data = glob.glob("data_*.csv")
    for f_data in arquivos_data:
        sessao_id = f_data.replace("data_", "").replace(".csv", "")
        base_dir = f"Analise_{sessao_id}"
        voltas_dir = os.path.join(base_dir, "Voltas")
        os.makedirs(voltas_dir, exist_ok=True)
        
        try:
            df = pd.read_csv(f_data)
            df_laps = pd.read_csv(f_data.replace('data_', 'laps_'))
            df_laps.columns = [c.strip() for c in df_laps.columns]
            df_laps['Time_sec'] = df_laps['Time'].apply(converter_tempo_sec)
            
            df_mov = df[df['Speed'] > 5].copy().reset_index(drop=True)
            df_mov['Dist'] = 0.0
            for v in df_mov['Lap'].unique():
                mask = df_mov['Lap'] == v
                df_mov.loc[mask, 'Dist'] = calcular_distancia_metros(df_mov[mask])

            top_laps_sorted = df_laps.sort_values('Time_sec')
            top_3_nums = top_laps_sorted['Lap'].head(3).tolist()
            mapa_fixo = gerar_mapa_master(df_mov, top_3_nums)
            
            ref_lap = df_mov[df_mov['Lap'] == top_3_nums[0]].copy().reset_index(drop=True)
            ref_dist_max = ref_lap['Dist'].max()
            ref_interp = interp1d(ref_lap['Dist'], (ref_lap['Timestamp_ms'] - ref_lap['Timestamp_ms'].min()) / 1000.0, bounds_error=False, fill_value="extrapolate")
            ref_speed_interp = interp1d(ref_lap['Dist'], ref_lap['Speed'], bounds_error=False, fill_value="extrapolate")
            
            apex_ref = {}
            for c in mapa_fixo:
                idx = (ref_lap['Dist'] - c['dist_ref']).abs().idxmin()
                apex_ref[c['id']] = ref_lap.loc[idx, 'Speed']

            # --- CÁLCULO DE VOLTA IDEAL ---
            best_sector_times = []
            for i in range(len(mapa_fixo)):
                parciais = []
                dist_fim = mapa_fixo[i]['dist_ref']
                dist_ini = mapa_fixo[i-1]['dist_ref'] if i > 0 else 0
                for v in df_laps['Lap']:
                    lap_pts = df_mov[df_mov['Lap'] == v]
                    if lap_pts.empty: continue
                    idx_ini = (lap_pts['Dist'] - dist_ini).abs().idxmin()
                    idx_fim = (lap_pts['Dist'] - dist_fim).abs().idxmin()
                    parciais.append((lap_pts.loc[idx_fim, 'Timestamp_ms'] - lap_pts.loc[idx_ini, 'Timestamp_ms'])/1000)
                valid_p = [p for p in parciais if p > 0]
                best_sector_times.append(min(valid_p) if valid_p else 0)
            volta_ideal = sum(best_sector_times)

            pdf = KartReport()
            pdf.set_margins(15, 15, 15); pdf.add_page()
            
            # --- PÁGINA 1: DEBRIEFING DO ENGENHEIRO ---
            pdf.set_fill_color(44, 62, 80); pdf.set_text_color(255, 255, 255); pdf.set_font('helvetica', 'B', 14)
            pdf.cell(0, 12, " RELATÓRIO DO ENGENHEIRO DE PISTA", fill=True, new_x=XPos.LMARGIN, new_y=YPos.NEXT)
            pdf.ln(5); pdf.set_text_color(0, 0, 0); pdf.set_font('helvetica', '', 11)
            pdf.set_x(15)
            pdf.multi_cell(0, 8, f"> PERFORMANCE GLOBAL: Recorde real de {df_laps['Time_sec'].min():.3f}s.")
            pdf.set_x(15)
            pdf.multi_cell(0, 8, f"> POTENCIAL OCULTO: Sua Volta Ideal e de {volta_ideal:.3f}s. Voce esta perdendo {df_laps['Time_sec'].min() - volta_ideal:.3f}s apenas por falta de constancia entre os setores.")
            
            pdf.ln(5); pdf.set_font('helvetica', 'B', 12); pdf.cell(0, 10, "INSIGHTS DE CONSISTÊNCIA POR CURVA", new_x=XPos.LMARGIN, new_y=YPos.NEXT)
            pdf.set_font('helvetica', '', 11)
            
            for c in mapa_fixo[:8]: 
                vels = []
                for v in df_mov['Lap'].unique():
                    if v == 0: continue
                    lap_sub = df_mov[df_mov['Lap']==v]
                    idx_l = (lap_sub['Dist']-c['dist_ref']).abs().idxmin()
                    vels.append(lap_sub.loc[idx_l, 'Speed'])
                osc = np.std(vels) if len(vels) > 1 else 0
                pdf.set_x(15); pdf.multi_cell(0, 7, f"- Curva {c['id']}: Variacao de {osc:.1f} km/h. " + ("Foco em estabilizar aqui!" if osc > 2.0 else "Traçado sólido."))

            # --- ANÁLISE POR VOLTA ---
            for v_num in sorted(df_mov['Lap'].unique()):
                if v_num == 0: continue
                lap_data = df_mov[df_mov['Lap'] == v_num].copy().reset_index(drop=True)
                
                curvas_volta = []
                for c in mapa_fixo:
                    idx_apex = (lap_data['Dist'] - c['dist_ref']).abs().idxmin()
                    idx_in = (lap_data['Dist'] - (c['dist_ref'] - 15)).abs().idxmin()
                    v_in = lap_data.loc[idx_in, 'Speed']
                    v_ap = lap_data.loc[max(0, idx_apex-8):min(len(lap_data)-1, idx_apex+8), 'Speed'].min()
                    curvas_volta.append({'id': c['id'], 'v_in': v_in, 'v_ap': v_ap, 'diff': v_ap - apex_ref.get(c['id'], v_ap), 'lat': c['lat'], 'lon': c['lon'], 'dist': c['dist_ref']})
                
                fig, axs = plt.subplot_mosaic([['map', 'map'], ['speed', 'delta']], figsize=(10, 8.5), gridspec_kw={'height_ratios': [1.4, 1]})
                axs['map'].scatter(lap_data['Lon'], lap_data['Lat'], c=lap_data['Speed'], cmap='RdYlGn', s=15, alpha=0.6)
                for c in curvas_volta:
                    axs['map'].annotate(f"C{c['id']}", (c['lon'], c['lat']), textcoords="offset points", xytext=(0,5), ha='center', fontsize=8, fontweight='bold', bbox=dict(boxstyle='circle,pad=0.2', fc='yellow', alpha=0.8, ec='none'))
                axs['map'].set_aspect('equal'); axs['map'].axis('off')
                
                dist_grid = np.linspace(0, max(lap_data['Dist'].max(), ref_dist_max), 250)
                speed_curr = interp1d(lap_data['Dist'], lap_data['Speed'], bounds_error=False, fill_value="extrapolate")(dist_grid)
                speed_ref = ref_speed_interp(dist_grid)
                axs['speed'].plot(dist_grid, speed_ref, color=COLOR_REF, lw=1, label='Ref', alpha=0.7)
                axs['speed'].plot(dist_grid, speed_curr, color=COLOR_LAP, lw=1.5, label='Atual')
                
                # ADICIONADO DE VOLTA: Labels C1, C2 no Speed Trace
                for c in curvas_volta:
                    axs['speed'].annotate(f"C{c['id']}", (c['dist'], c['v_ap']), textcoords="offset points", 
                                         xytext=(0, 8), ha='center', fontsize=6, fontweight='bold', color='red')
                    axs['speed'].scatter(c['dist'], c['v_ap'], color='red', s=10, zorder=5)
                
                axs['speed'].set_title("Speed Trace (km/h)"); axs['speed'].set_xlabel("Distancia (m)")
                
                time_curr = (lap_data['Timestamp_ms'] - lap_data['Timestamp_ms'].min()) / 1000.0
                delta = interp1d(lap_data['Dist'], time_curr, bounds_error=False, fill_value="extrapolate")(dist_grid) - ref_interp(dist_grid)
                axs['delta'].plot(dist_grid, delta, color=COLOR_DELTA, lw=1.5); axs['delta'].axhline(0, color='black', lw=0.8, ls='--')
                axs['delta'].fill_between(dist_grid, delta, 0, where=(delta > 0), color='#e74c3c', alpha=0.3)
                axs['delta'].fill_between(dist_grid, delta, 0, where=(delta < 0), color='#2ecc71', alpha=0.3)
                axs['delta'].set_title("Delta Time (s)"); axs['delta'].set_xlabel("Distancia (m)")
                
                img_path = os.path.join(voltas_dir, f"V_{v_num}.png")
                plt.tight_layout(); plt.savefig(img_path, dpi=130); plt.close()
                
                pdf.add_page(); pdf.image(img_path, x=15, y=32, w=180) 
                pdf.set_xy(15, 188); pdf.set_font('helvetica', 'B', 12)
                t_v = df_laps[df_laps['Lap']==v_num]['Time'].iloc[0]
                pdf.cell(0, 10, f"VOLTA {int(v_num)} | Tempo: {t_v}s", align='C', new_x=XPos.LMARGIN, new_y=YPos.NEXT)
                
                if curvas_volta:
                    pdf.set_y(200); pdf.set_font('helvetica', 'B', 7); pdf.set_fill_color(240, 240, 240)
                    cols = 12 
                    for i in range(0, len(curvas_volta), cols):
                        linha = curvas_volta[i:i+cols]; cw = 180 / len(linha)
                        pdf.set_x(15); [pdf.cell(cw, 5.5, f"C{c['id']}", 1, 0, 'C', True) for c in linha]
                        pdf.ln(); pdf.set_x(15); pdf.set_font('helvetica', '', 6)
                        [pdf.cell(cw, 4.5, f"In:{c['v_in']:.1f}", 1, 0, 'C') for c in linha]
                        pdf.ln(); pdf.set_x(15); pdf.set_font('helvetica', 'B', 6)
                        for c in linha:
                            pdf.set_text_color(46, 204, 113) if c['diff'] > 0.5 else pdf.set_text_color(231, 76, 60) if c['diff'] < -0.5 else pdf.set_text_color(0,0,0)
                            pdf.cell(cw, 5, f"Ap:{c['v_ap']:.1f}", 1, 0, 'C')
                        pdf.ln(); pdf.set_text_color(0,0,0); pdf.set_x(15)
                        [pdf.cell(cw, 4, f"{'+' if c['diff']>0 else ''}{c['diff']:.1f}", 1, 0, 'C') for c in linha]
                        pdf.ln(7)

            # --- PÁGINA FINAL: RANKING ---
            pdf.add_page()
            pdf.set_fill_color(44, 62, 80); pdf.set_text_color(255, 255, 255); pdf.set_font('helvetica', 'B', 16)
            pdf.cell(0, 15, " RANKING FINAL E RESUMO DA SESSÃO", fill=True, align='C', new_x=XPos.LMARGIN, new_y=YPos.NEXT)
            pdf.ln(10); pdf.set_text_color(0, 0, 0)

            pdf.set_font('helvetica', 'B', 12); pdf.cell(0, 10, "TOP 10 MELHORES VOLTAS:", new_x=XPos.LMARGIN, new_y=YPos.NEXT)
            pdf.set_font('helvetica', 'B', 10); pdf.set_fill_color(230, 230, 230)
            col_w = pdf.epw / 4
            for h in ["RANK", "VOLTA", "TEMPO", "V. MÁX"]: pdf.cell(col_w, 10, h, 1, 0, 'C', True)
            pdf.ln(); pdf.set_font('helvetica', '', 10)
            
            for i, (idx, row) in enumerate(top_laps_sorted.head(10).iterrows(), 1):
                v_max_lap = df_mov[df_mov['Lap']==row['Lap']]['Speed'].max()
                pdf.cell(col_w, 10, f"{i} Lugar", 1, 0, 'C')
                pdf.cell(col_w, 10, str(int(row['Lap'])), 1, 0, 'C')
                pdf.cell(col_w, 10, f"{row['Time']}s", 1, 0, 'C')
                pdf.cell(col_w, 10, f"{v_max_lap:.1f} km/h", 1, 1, 'C')

            pdf.ln(10); pdf.set_font('helvetica', 'B', 12); pdf.cell(0, 10, "CONSIDERAÇÕES FINAIS DO ENGENHEIRO:", new_x=XPos.LMARGIN, new_y=YPos.NEXT)
            pdf.set_font('helvetica', 'I', 11); pdf.set_x(15)
            pdf.multi_cell(0, 8, "Analise os graficos de Speed Trace para identificar se voce esta atrasando muito a retomada de aceleracao. O potencial existe, mas os setores ideais mostram que voce pode baixar significativamente o tempo se conectar os melhores trechos.")

            pdf.output(os.path.join(base_dir, f"Relatorio_Final_Pro_v48_{sessao_id}.pdf"))
            print(f">>> Relatório v4.8 Gerado! Etiquetas recuperadas no Speed Trace.")
            
        except Exception as e:
            print(f"Erro: {e}"); import traceback; traceback.print_exc()

if __name__ == "__main__":
    processar_sessao()