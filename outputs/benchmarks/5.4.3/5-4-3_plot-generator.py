import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import numpy as np
import sys
import os

# Füge Root-Benchmark-Ordner zum Pfad hinzu für benchmark_utils
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))
import benchmark_utils as bu

def generate_5_4_3_plots(csv_path):
    output_dir = os.path.dirname(csv_path) or '.'
    
    print(f"Verarbeite Datensatz 5.4.3: {csv_path}")
    df = pd.read_csv(csv_path)
    
    df = bu.clean_arch(df)
    
    # ---------------------------------------------------------
    # DATA PRE-PROCESSING: OVERDRAW MAPPING (Forward Baseline)
    # ---------------------------------------------------------
    df_forward = df[df['Architecture'] == 'Forward'].copy()
    overdraw_map = df_forward.groupby('StepValue', observed=False)['OverdrawFactor'].mean().to_dict()
    
    step_values_sorted = sorted(df['StepValue'].unique())
    overdraw_labels = []
    for v in step_values_sorted:
        if v in overdraw_map:
            overdraw_labels.append(f"{overdraw_map[v]:.1f}")
        else:
            overdraw_labels.append("N/A")
            
    df['Overdraw_Label'] = df['StepValue'].map(lambda x: f"{overdraw_map.get(x, 0):.1f}")
    df['Overdraw_Label'] = pd.Categorical(df['Overdraw_Label'], categories=overdraw_labels, ordered=True)

    # Ausreißer-Korrektur (3 Sigma)
    df = bu.filter_outliers_3sigma(df, ['Overdraw_Label', 'Architecture'], ['TotalGpuMs'])
    
    bu.setup_latex_font()

    # ---------------------------------------------------------
    # FIGURE 2: Stacked Bar Chart (Decoupling Proof)
    # ---------------------------------------------------------
    archs_g2 = ['Forward', 'Deferred_Uber', 'Deferred_Volume']
    df_g2_raw = df[df['Architecture'].isin(archs_g2)].copy()
    
    df_g2 = df_g2_raw.groupby(['Overdraw_Label', 'Architecture'], sort=False, observed=False)[['GeomMs', 'LightMs', 'TotalGpuMs']].mean().reset_index()
    df_g2['OverheadMs'] = df_g2['TotalGpuMs'] - (df_g2['GeomMs'] + df_g2['LightMs'])
    
    fig, ax = plt.subplots(figsize=(14, 7))
    
    x = np.arange(len(overdraw_labels))
    width = 0.25
    color_overhead = '#D3D3D3' # Light Gray
    
    for i, arch in enumerate(archs_g2):
        arch_data = df_g2[df_g2['Architecture'] == arch].set_index('Overdraw_Label').reindex(overdraw_labels)
        base_color = bu.ARCH_COLOR_MAP[arch] 
        
        offset = x + (i - 1) * width
        
        geom = arch_data['GeomMs'].fillna(0).values
        light = arch_data['LightMs'].fillna(0).values
        overhead = arch_data['OverheadMs'].fillna(0).values
        
        if arch == 'Forward':
            ax.bar(offset, light, width, color=base_color, alpha=0.8, edgecolor='black', linewidth=0.5)
            bottom_overhead = light
            
            for j in range(len(x)):
                if light[j] > 0.01:
                    ax.text(offset[j], light[j]/2, f"{light[j]:.2f}", ha='center', va='center', fontsize=9)
        else:
            ax.bar(offset, geom, width, color=base_color, alpha=0.4, edgecolor='black', linewidth=0.5)
            ax.bar(offset, light, width, bottom=geom, color=base_color, alpha=0.8, edgecolor='black', linewidth=0.5)
            bottom_overhead = geom + light
            
            for j in range(len(x)):
                if geom[j] > 0.01:
                    ax.text(offset[j], geom[j]/2, f"{geom[j]:.2f}", ha='center', va='center', fontsize=9)
                if light[j] > 0.01:
                    ax.text(offset[j], geom[j] + light[j]/2, f"{light[j]:.2f}", ha='center', va='center', fontsize=9)
        
        ax.bar(offset, overhead, width, bottom=bottom_overhead, color=color_overhead, edgecolor='black', linewidth=0.5)
        for j in range(len(x)):
            if overhead[j] > 0.01:
                ax.text(offset[j], bottom_overhead[j] + overhead[j]/2, f"{overhead[j]:.2f}", ha='center', va='center', fontsize=9)

    ax.set_xticks(x)
    ax.set_xticklabels(overdraw_labels)
    ax.set_xlabel('Theoretischer Overdraw Faktor (Geometrie)')
    ax.set_ylabel('GPU-Ausführungszeit (ms)')
    
    bu.apply_strict_styling(ax, force_zero_y=False)
    
    from matplotlib.patches import Patch
    legend_elements = [
        Patch(facecolor='gray', alpha=0.8, edgecolor='black', label='Forward: Geometrie + Beleuchtung (Kombiniert)'),
        Patch(facecolor='gray', alpha=0.4, edgecolor='black', label='Deferred: Geometrie (G-Buffer)'),
        Patch(facecolor='gray', alpha=0.8, edgecolor='black', label='Deferred: Beleuchtung (Shading)'),
        Patch(facecolor=color_overhead, edgecolor='black', label='Engine Overhead (OverheadMs)')
    ]
    ax.legend(handles=legend_elements, loc='upper left', framealpha=0.8, facecolor='white', edgecolor='gray')
    
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, '5_4_3_decoupling_proof.pdf'), format='pdf', bbox_inches='tight')
    plt.close()
    print("-> Graph 2 exportiert (5_4_3_decoupling_proof.pdf)")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python 5-4-3_plot-generator.py <path_to_csv>")
        sys.exit(1)
        
    csv_file = sys.argv[1]
    if not os.path.exists(csv_file):
        print(f"Fehler: Datei nicht gefunden: {csv_file}")
        sys.exit(1)
        
    generate_5_4_3_plots(csv_file)
