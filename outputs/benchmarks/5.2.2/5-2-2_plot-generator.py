import pandas as pd
import matplotlib.pyplot as plt
from matplotlib.lines import Line2D
import seaborn as sns
import sys
import os

# Füge Root-Benchmark-Ordner zum Pfad hinzu für benchmark_utils
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))
import benchmark_utils as bu

def format_instances(val):
    mapping = {
        1000: '1K',
        5000: '5K',
        10000: '10K',
        25000: '25K',
        50000: '50K'
    }
    return mapping.get(val, str(val))

def generate_5_2_2_plots(csv_path):
    output_dir = os.path.dirname(csv_path) or '.'
    
    print(f"Verarbeite Datensatz 5.2.2: {csv_path}")
    df = pd.read_csv(csv_path)
    
    df = df.sort_values(by='Instances', ascending=True)
    df['Instances_Label'] = df['Instances'].apply(format_instances)
    
    # Architektur Standardisierung
    df = bu.clean_arch(df)
    
    # Ausreißer-Korrektur (3 Sigma) auf Ebene Instances & Architektur
    df = bu.filter_outliers_3sigma(df, ['Instances_Label', 'Architecture'], ['CpuLogicMs', 'CpuRenderMs', 'TotalFrameMs'])
    
    instance_order = ['1K', '5K', '10K', '25K', '50K']
    actual_archs = df['Architecture'].dropna().unique()
    
    bu.setup_latex_font()
    
    # ---------------------------------------------------------
    # GRAPH 1: CPU Dispatch vs. Logic Scaling
    # ---------------------------------------------------------
    df_g1 = df.groupby(['Instances_Label', 'Architecture'], sort=False, observed=False).agg({
        'CpuLogicMs': 'mean',
        'CpuRenderMs': 'mean'
    }).reset_index()
    
    fig, ax = plt.subplots(figsize=(10, 6))
    
    for arch in actual_archs:
        arch_data = df_g1[df_g1['Architecture'] == arch].set_index('Instances_Label').reindex(instance_order)
        color = bu.ARCH_COLOR_MAP.get(arch, '#000000')
        
        # CpuLogicMs: Solid line, circle markers
        ax.plot(arch_data.index, arch_data['CpuLogicMs'], 
                color=color, linestyle='-', marker='o', linewidth=2.5)
        
        # CpuRenderMs: Dashed line, triangle markers
        ax.plot(arch_data.index, arch_data['CpuRenderMs'], 
                color=color, linestyle='--', marker='^', linewidth=2.5)
        
    ax.set_xlabel('Aktive Instanzen (Instances)')
    ax.set_ylabel('Ausführungszeit (ms)')
    bu.apply_strict_styling(ax, force_zero_y=True)
    
    # Kombinierte Legende erstellen
    legend_elements_arch = [Line2D([0], [0], color=bu.ARCH_COLOR_MAP.get(a, '#000000'), lw=3, label=a) for a in actual_archs]
    legend_elements_metric = [
        Line2D([0], [0], color='black', lw=2, linestyle='-', marker='o', label='CpuLogicMs'),
        Line2D([0], [0], color='black', lw=2, linestyle='--', marker='^', label='CpuRenderMs')
    ]
    
    # Alle Elemente in eine einzige Liste packen
    all_handles = legend_elements_arch + legend_elements_metric
    
    # Eine gemeinsame Legende oben links platzieren
    ax.legend(handles=all_handles, title='Architektur & Metrik', loc='upper left', framealpha=0.8, facecolor='white', edgecolor='gray')
    
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, '5_2_2_cpu_scaling_logic_vs_render.pdf'), format='pdf', bbox_inches='tight')
    plt.close()
    print("-> Graph 1 exportiert (5_2_2_cpu_scaling_logic_vs_render.pdf)")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python 5-2-2_plot-generator.py <path_to_csv>")
        sys.exit(1)
        
    csv_file = sys.argv[1]
    if not os.path.exists(csv_file):
        print(f"Fehler: Datei nicht gefunden: {csv_file}")
        sys.exit(1)
        
    generate_5_2_2_plots(csv_file)
