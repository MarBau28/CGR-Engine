import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import numpy as np
import sys
import os

# Füge Root-Benchmark-Ordner zum Pfad hinzu für benchmark_utils
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))
import benchmark_utils as bu

def generate_5_3_1_plots(csv_path):
    output_dir = os.path.dirname(csv_path) or '.'
    
    print(f"Verarbeite Datensatz 5.3.1: {csv_path}")
    df = pd.read_csv(csv_path)
    
    # ---------------------------------------------------------
    # CRITICAL DATA MAPPING
    # ---------------------------------------------------------
    resolution_mapping = {
        0.0: '480p',
        1.0: '720p',
        2.0: '1080p',
        3.0: '1440p',
        4.0: '4K'
    }
    df['Resolution'] = df['StepValue'].map(resolution_mapping)
    resolution_order = ['480p', '720p', '1080p', '1440p', '4K']
    
    df = bu.clean_arch(df)
    
    # Ausreißer-Korrektur (3 Sigma)
    df = bu.filter_outliers_3sigma(df, ['Resolution', 'Architecture'], ['TotalGpuMs'])
    
    actual_archs = df['Architecture'].dropna().unique()
    bu.setup_latex_font()
    
    # ---------------------------------------------------------
    # GRAPH 1: Resolution Scaling & Crossover
    # ---------------------------------------------------------
    df_g1 = df.groupby(['Resolution', 'Architecture'], sort=False, observed=False)['TotalGpuMs'].mean().reset_index()
    
    fig, ax = plt.subplots(figsize=(10, 6))
    
    for arch in actual_archs:
        arch_data = df_g1[df_g1['Architecture'] == arch].set_index('Resolution').reindex(resolution_order)
        color = bu.ARCH_COLOR_MAP.get(arch, '#000000')
        ax.plot(arch_data.index, arch_data['TotalGpuMs'], 
                color=color, linestyle='-', marker='o', linewidth=2.5, label=arch)
        
    ax.set_xlabel('Auflösung')
    ax.set_ylabel('GPU-Ausführungszeit (ms)')
    bu.apply_strict_styling(ax, force_zero_y=True)
    ax.legend(title='Architektur', loc='upper left', framealpha=0.8, facecolor='white', edgecolor='gray')
    
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, '5_3_1_resolution_scaling_crossover.pdf'), format='pdf', bbox_inches='tight')
    plt.close()
    print("-> Graph 1 exportiert (5_3_1_resolution_scaling_crossover.pdf)")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python 5-3-1_plot-generator.py <path_to_csv>")
        sys.exit(1)
        
    csv_file = sys.argv[1]
    if not os.path.exists(csv_file):
        print(f"Fehler: Datei nicht gefunden: {csv_file}")
        sys.exit(1)
        
    generate_5_3_1_plots(csv_file)
