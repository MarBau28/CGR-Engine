import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import numpy as np
import sys
import os
import matplotlib.patches

# Füge Root-Benchmark-Ordner zum Pfad hinzu für benchmark_utils
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))
import benchmark_utils as bu

def generate_5_4_2_plots(csv_path):
    output_dir = os.path.dirname(csv_path) or '.'
    
    print(f"Verarbeite Datensatz 5.4.2: {csv_path}")
    df = pd.read_csv(csv_path)
    
    df = bu.clean_arch(df)
    
    # X-Achsen Werte als diskrete Kategorien behandeln
    step_values_sorted = sorted(df['StepValue'].unique())
    step_labels = [f"{val:.1f}" for val in step_values_sorted]
    
    step_mapping = dict(zip(step_values_sorted, step_labels))
    df['Intensity_Label'] = df['StepValue'].map(step_mapping)
    df['Intensity_Label'] = pd.Categorical(df['Intensity_Label'], categories=step_labels, ordered=True)

    # Ausreißer-Korrektur (3 Sigma)
    df = bu.filter_outliers_3sigma(df, ['Intensity_Label', 'Architecture'], ['TotalGpuMs'])
    
    bu.setup_latex_font()

    # ---------------------------------------------------------
    # FIGURE 1: Line Chart (Macro-Trend / Means)
    # ---------------------------------------------------------
    df_g1 = df.groupby(['Intensity_Label', 'Architecture'], sort=False, observed=False)['TotalGpuMs'].mean().reset_index()
    
    fig, ax = plt.subplots(figsize=(10, 6))
    
    for arch in bu.ARCH_ORDER:
        arch_data = df_g1[df_g1['Architecture'] == arch].set_index('Intensity_Label').reindex(step_labels)
        arch_data = arch_data.dropna(subset=['TotalGpuMs']) 
        
        if not arch_data.empty:
            color = bu.ARCH_COLOR_MAP[arch]
            ax.plot(arch_data.index, arch_data['TotalGpuMs'], 
                    color=color, linestyle='-', marker='o', linewidth=2, markersize=6, 
                    label=arch)
            
    ax.set_xlabel('Lichtintensität (Radius-Multiplikator)')
    ax.set_ylabel('GPU-Ausführungszeit (ms)')
    
    bu.apply_strict_styling(ax, force_zero_y=True)
    ax.legend(title='Architektur', loc='upper left', framealpha=0.8, facecolor='white', edgecolor='gray')
    
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, '5-4-2-1_LightIntensity_Means.pdf'), format='pdf', bbox_inches='tight')
    plt.close()
    print("-> Graph 1 exportiert (5-4-2_LightIntensity_Means.pdf)")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python 5-4-2-1_plot-generator.py <path_to_csv>")
        sys.exit(1)
        
    csv_file = sys.argv[1]
    if not os.path.exists(csv_file):
        print(f"Fehler: Datei nicht gefunden: {csv_file}")
        sys.exit(1)
        
    generate_5_4_2_plots(csv_file)
