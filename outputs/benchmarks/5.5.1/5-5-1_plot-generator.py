import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import sys
import os
from matplotlib.patches import Patch

# Füge Root-Benchmark-Ordner zum Pfad hinzu für benchmark_utils
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))
import benchmark_utils as bu

def generate_5_5_1_plots(csv_path):
    output_dir = os.path.dirname(csv_path) or '.'
    
    print(f"Verarbeite Datensatz Spatial Entropy: {csv_path}")
    df = pd.read_csv(csv_path)
    
    df = bu.clean_arch(df)
    
    # Ausreißer-Korrektur (3 Sigma) auf Ebene StepValue (Geklustert vs Gestreut)
    df = bu.filter_outliers_3sigma(df, ['StepValue', 'Architecture'], ['LightMs'])
    
    bu.setup_latex_font()

    # ---------------------------------------------------------
    # FIGURE 1: Grouped Bar Chart (Phase 1)
    # ---------------------------------------------------------
    df_g1 = df[df['StepValue'].isin([0.0, 1.0])].copy()
    if not df_g1.empty:
        df_g1['EntropyState'] = df_g1['StepValue'].map({0.0: 'Geklustert', 1.0: 'Gestreut'})
        df_g1_agg = df_g1.groupby(['Architecture', 'EntropyState'], sort=False, observed=False)['LightMs'].mean().reset_index()
        
        fig, ax = plt.subplots(figsize=(10, 6))
        
        x = np.arange(len(bu.ARCH_ORDER))
        width = 0.35
        
        for i, arch in enumerate(bu.ARCH_ORDER):
            arch_data = df_g1_agg[df_g1_agg['Architecture'] == arch]
            color = bu.ARCH_COLOR_MAP.get(arch, '#000000')
            
            # Geklustert (0.0)
            val_clustered = arch_data[arch_data['EntropyState'] == 'Geklustert']['LightMs'].values
            if len(val_clustered) > 0:
                ax.bar(x[i] - width/2, val_clustered[0], width, color=color, edgecolor='black', linewidth=1.0)
                
            # Gestreut (1.0)
            val_scattered = arch_data[arch_data['EntropyState'] == 'Gestreut']['LightMs'].values
            if len(val_scattered) > 0:
                ax.bar(x[i] + width/2, val_scattered[0], width, color=color, alpha=0.6, hatch='//', edgecolor='black', linewidth=1.0)

        ax.set_xticks(x)
        ax.set_xticklabels(bu.ARCH_ORDER)
        ax.set_xlabel('Rendering-Pipeline')
        ax.set_ylabel('GPU Shading Zeit (ms)')
        
        bu.apply_strict_styling(ax, force_zero_y=True)
        bu.annotate_bar_chart(ax, fontsize=10)
        
        legend_elements = [
            Patch(facecolor='gray', edgecolor='black', label='Räumliche Entropie: Geklustert'),
            Patch(facecolor='gray', alpha=0.6, hatch='//', edgecolor='black', label='Räumliche Entropie: Gestreut')
        ]
        ax.legend(handles=legend_elements, loc='upper left', framealpha=0.8, facecolor='white', edgecolor='gray')
        
        plt.tight_layout()
        plt.savefig(os.path.join(output_dir, '5_5_1_entropy_divergence_means.pdf'), format='pdf', bbox_inches='tight')
        plt.close()
        print("-> Graph 1 exportiert (5_5_1_entropy_divergence_means.pdf)")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python 5-5-1_plot-generator.py <path_to_csv>")
        sys.exit(1)
        
    csv_file = sys.argv[1]
    if not os.path.exists(csv_file):
        print(f"Fehler: Datei nicht gefunden: {csv_file}")
        sys.exit(1)
        
    generate_5_5_1_plots(csv_file)
