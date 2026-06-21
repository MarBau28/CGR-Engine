import pandas as pd
import matplotlib.pyplot as plt
from matplotlib.lines import Line2D
import seaborn as sns
import sys
import os

# Füge Root-Benchmark-Ordner zum Pfad hinzu für benchmark_utils
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))
import benchmark_utils as bu

def generate_5_2_3_plots(csv_path):
    output_dir = os.path.dirname(csv_path) or '.'
    
    print(f"Verarbeite Datensatz 5.2.3: {csv_path}")
    df = pd.read_csv(csv_path)
    
    df = bu.clean_arch(df)
    
    # ---------------------------------------------------------
    # GRAPH 1: Fill-Rate Saturation
    # ---------------------------------------------------------
    # Zunächst OverdrawFactor pro StepValue aggregieren
    step_overdraw = df.groupby('StepValue', observed=False)['OverdrawFactor'].mean().reset_index()
    # Labels auf 1 Nachkommastelle runden
    step_overdraw['Overdraw_Label'] = step_overdraw['OverdrawFactor'].apply(lambda x: f"{x:.1f}")
    
    # Sortieren nach StepValue, um korrekte Reihenfolge der Labels zu garantieren
    step_overdraw = step_overdraw.sort_values('StepValue')
    overdraw_order = step_overdraw['Overdraw_Label'].tolist()
    step_to_label = dict(zip(step_overdraw['StepValue'], step_overdraw['Overdraw_Label']))
    
    df['Overdraw_Label'] = df['StepValue'].map(step_to_label)
    
    # Ausreißer-Korrektur (3 Sigma)
    df = bu.filter_outliers_3sigma(df, ['Overdraw_Label', 'Architecture'], ['TotalGpuMs'])
    
    actual_archs = df['Architecture'].dropna().unique()
    bu.setup_latex_font()
    
    # Daten für Graph 1 aggregieren
    df_g1 = df.groupby(['Overdraw_Label', 'Architecture'], sort=False, observed=False)['TotalGpuMs'].mean().reset_index()
    
    fig, ax = plt.subplots(figsize=(10, 6))
    
    for arch in actual_archs:
        # Reindexing based on overdraw_order um konsistente Kategorien auf X-Achse sicherzustellen
        arch_data = df_g1[df_g1['Architecture'] == arch].set_index('Overdraw_Label').reindex(overdraw_order)
        color = bu.ARCH_COLOR_MAP.get(arch, '#000000')
        
        ax.plot(arch_data.index, arch_data['TotalGpuMs'], 
                color=color, linestyle='-', marker='o', linewidth=2.5, 
                label=arch)
        
    ax.set_xlabel('Theoretischer Overdraw Faktor (Schichten pro Pixel)')
    ax.set_ylabel('GPU-Ausführungszeit (ms)')
    bu.apply_strict_styling(ax, force_zero_y=True)
    
    # Legende einfügen
    ax.legend(title='Architektur', loc='upper left', framealpha=0.8, facecolor='white', edgecolor='gray')
    
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, '5_2_3_overdraw_fillrate_scaling.pdf'), format='pdf', bbox_inches='tight')
    plt.close()
    print("-> Graph 1 exportiert (5_2_3_overdraw_fillrate_scaling.pdf)")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python 5-2-3_plot-generator.py <path_to_csv>")
        sys.exit(1)
        
    csv_file = sys.argv[1]
    if not os.path.exists(csv_file):
        print(f"Fehler: Datei nicht gefunden: {csv_file}")
        sys.exit(1)
        
    generate_5_2_3_plots(csv_file)
