import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import sys
import os

# Füge Root-Benchmark-Ordner zum Pfad hinzu für benchmark_utils
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))
import benchmark_utils as bu

def generate_5_5_2_plots(csv_path):
    output_dir = os.path.dirname(csv_path) or '.'
    
    print(f"Verarbeite Datensatz Style Combinatorics: {csv_path}")
    df = pd.read_csv(csv_path)
    
    df = bu.clean_arch(df)

    # Mapping für Stile
    style_mapping = {0.0: "1 Stil", 1.0: "2 Stile", 2.0: "3 Stile"}
    style_order = ["1 Stil", "2 Stile", "3 Stile"]
    df['Style_Label'] = df['StepValue'].map(style_mapping)
    df['Style_Label'] = pd.Categorical(df['Style_Label'], categories=style_order, ordered=True)

    # Ausreißer-Korrektur (3 Sigma)
    df = bu.filter_outliers_3sigma(df, ['Style_Label', 'Architecture'], ['LightMs', 'CpuLogicMs'])

    bu.setup_latex_font()

    # ---------------------------------------------------------
    # FIGURE 1: Grouped Bar Chart (GPU Shading Degradation)
    # ---------------------------------------------------------
    df_g1 = df.groupby(['Style_Label', 'Architecture'], sort=False, observed=False)['LightMs'].mean().reset_index()
    
    fig, ax = plt.subplots(figsize=(10, 6))
    
    x = np.arange(len(style_order))
    width = 0.25
    
    for i, arch in enumerate(bu.ARCH_ORDER):
        arch_data = df_g1[df_g1['Architecture'] == arch].set_index('Style_Label').reindex(style_order)
        color = bu.ARCH_COLOR_MAP[arch]
        
        offset = (i - 1) * width  # zentriert bei i=1
        
        ax.bar(x + offset, arch_data['LightMs'].fillna(0).values, width, 
               label=arch, color=color, edgecolor='black', linewidth=1.0)

    ax.set_xticks(x)
    ax.set_xticklabels(style_order)
    ax.set_xlabel('Anzahl aktiver Rendering-Stile')
    ax.set_ylabel('GPU Shading Zeit (ms)')
    
    bu.apply_strict_styling(ax, force_zero_y=True)
    bu.annotate_bar_chart(ax, fontsize=10)
    ax.legend(title='Rendering-Pipeline', loc='upper left', framealpha=0.8, facecolor='white', edgecolor='gray')
    
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, '5_5_2_style_combinatorics_gpu.pdf'), format='pdf', bbox_inches='tight')
    plt.close()
    print("-> Graph 1 exportiert (5_5_2_style_combinatorics_gpu.pdf)")

    # ---------------------------------------------------------
    # FIGURE 2: Line Chart (CPU Dispatch Scaling)
    # ---------------------------------------------------------
    df_g2 = df.groupby(['Style_Label', 'Architecture'], sort=False, observed=False)['CpuLogicMs'].mean().reset_index()
    
    fig, ax = plt.subplots(figsize=(10, 6))
    
    for arch in bu.ARCH_ORDER:
        arch_data = df_g2[df_g2['Architecture'] == arch].set_index('Style_Label').reindex(style_order)
        color = bu.ARCH_COLOR_MAP[arch]
        
        # Linie zeichnen
        ax.plot(arch_data.index, arch_data['CpuLogicMs'].values, 
                color=color, linestyle='-', marker='o', linewidth=2, markersize=6, 
                label=arch)

    ax.set_xlabel('Anzahl aktiver Rendering-Stile')
    ax.set_ylabel('CPU Dispatch Zeit (ms)')
    
    bu.apply_strict_styling(ax, force_zero_y=True)
    ax.legend(title='Rendering-Pipeline', loc='upper left', framealpha=0.8, facecolor='white', edgecolor='gray')
    
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, '5_5_2_style_combinatorics_cpu.pdf'), format='pdf', bbox_inches='tight')
    plt.close()
    print("-> Graph 2 exportiert (5_5_2_style_combinatorics_cpu.pdf)")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python 5-5-2_plot-generator.py <path_to_csv>")
        sys.exit(1)
        
    csv_file = sys.argv[1]
    if not os.path.exists(csv_file):
        print(f"Fehler: Datei nicht gefunden: {csv_file}")
        sys.exit(1)
        
    generate_5_5_2_plots(csv_file)
