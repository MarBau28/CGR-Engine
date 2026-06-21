import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import numpy as np
import sys
import os
import matplotlib.patches
from matplotlib.patches import Patch
from matplotlib.lines import Line2D

# Füge Root-Benchmark-Ordner zum Pfad hinzu für benchmark_utils
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))
import benchmark_utils as bu

def generate_5_3_2_plots(csv_path):
    output_dir = os.path.dirname(csv_path) or '.'
    
    print(f"Verarbeite Datensatz 5.3.2: {csv_path}")
    df = pd.read_csv(csv_path)
    
    df = bu.clean_arch(df)
    
    # Ausreißer-Korrektur (3 Sigma)
    df = bu.filter_outliers_3sigma(df, ['Architecture', 'StepValue'], ['TotalGpuMs'])
    
    bu.setup_latex_font()
    
    # ---------------------------------------------------------
    # FIGURE 1: Grouped Bar Chart (Macro-Trend / Means)
    # ---------------------------------------------------------
    df_g1 = df.groupby(['Architecture', 'StepValue'], sort=False, observed=False)['TotalGpuMs'].mean().reset_index()
    
    fig, ax = plt.subplots(figsize=(10, 6))
    
    x = np.arange(len(bu.ARCH_ORDER))
    width = 0.35
    
    for i, arch in enumerate(bu.ARCH_ORDER):
        arch_data = df_g1[df_g1['Architecture'] == arch]
        color = bu.ARCH_COLOR_MAP.get(arch, '#000000')
        
        # 0.0 (Ohne Boden) -> alpha=0.4, hatch='///'
        val_0 = arch_data[arch_data['StepValue'] == 0.0]['TotalGpuMs'].values
        if len(val_0) > 0:
            ax.bar(x[i] - width/2, val_0[0], width, color=color, alpha=0.4, hatch='///', edgecolor=color, linewidth=1.5)
            
        # 1.0 (Mit Boden) -> solid
        val_1 = arch_data[arch_data['StepValue'] == 1.0]['TotalGpuMs'].values
        if len(val_1) > 0:
            ax.bar(x[i] + width/2, val_1[0], width, color=color, alpha=1.0, edgecolor='black', linewidth=1.0)
            
    ax.set_xticks(x)
    ax.set_xticklabels(bu.ARCH_ORDER)
    ax.set_xlabel('Rendering-Architektur')
    ax.set_ylabel('GPU-Ausführungszeit (ms)')
    
    bu.apply_strict_styling(ax, force_zero_y=True)
    bu.annotate_bar_chart(ax, fontsize=10)
    
    legend_elements_1 = [
        Patch(facecolor='#D3D3D3', alpha=0.4, hatch='///', edgecolor='gray', label='Ohne Boden (Basis-Overhead)'),
        Patch(facecolor='#D3D3D3', alpha=1.0, edgecolor='black', label='Mit Boden (Bandbreiten-Limit)')
    ]
    ax.legend(handles=legend_elements_1, loc='upper left', framealpha=0.8, facecolor='white')
    
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, '5-3-2_BaseBandwidthTax_Means.pdf'), format='pdf', bbox_inches='tight')
    plt.close()
    print("-> Graph 1 exportiert (5-3-2_BaseBandwidthTax_Means.pdf)")

    # ---------------------------------------------------------
    # FIGURE 2: Grouped Box Plot (Micro-Trend / Variance)
    # ---------------------------------------------------------
    fig, ax = plt.subplots(figsize=(10, 6))
    
    df_box = df.copy()
    df_box['StepCategory'] = df_box['StepValue'].astype(str)
    
    if not df_box.empty:
        sns.boxplot(data=df_box, x='Architecture', y='TotalGpuMs', hue='StepCategory', 
                    ax=ax, whis=(1, 99), 
                    flierprops={'markersize': 2, 'alpha': 0.5, 'marker': 'o', 'markerfacecolor': 'black', 'markeredgecolor': 'none'})
                    
        # Styling for individual patches (boxes)
        boxes = [patch for patch in ax.patches if type(patch) == matplotlib.patches.PathPatch]
        
        if len(boxes) == 6: # 3 Architekturen * 2 Hues
            for i, arch in enumerate(bu.ARCH_ORDER):
                base_color = bu.ARCH_COLOR_MAP[arch]
                
                box_0 = boxes[i]
                box_0.set_facecolor('none')  # Transparent
                box_0.set_edgecolor(base_color)
                box_0.set_linestyle('--')
                box_0.set_linewidth(2)
                
                box_1 = boxes[i + 3]
                box_1.set_facecolor(base_color)
                box_1.set_edgecolor('black')
                box_1.set_linewidth(1.5)
                box_1.set_linestyle('-')

        ax.set_xlabel('Rendering-Architektur')
        ax.set_ylabel('GPU-Ausführungszeit (ms)')
        
        bu.apply_strict_styling(ax, force_zero_y=False)
        bu.annotate_boxplot(ax, df_box, 'Architecture', 'TotalGpuMs', hue_col='StepCategory')
        
        legend_elements_2 = [
            Line2D([0], [0], color='gray', linestyle='--', lw=2, label='Verteilung Ohne Boden'),
            Patch(facecolor='gray', edgecolor='black', label='Verteilung Mit Boden')
        ]
        ax.legend(handles=legend_elements_2, loc='upper left', framealpha=0.8, facecolor='white')
        
        plt.tight_layout()
        plt.savefig(os.path.join(output_dir, '5-3-2_BaseBandwidthTax_Variance.pdf'), format='pdf', bbox_inches='tight')
        plt.close()
        print("-> Graph 2 exportiert (5-3-2_BaseBandwidthTax_Variance.pdf)")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python 5-3-2_plot-generator.py <path_to_csv>")
        sys.exit(1)
        
    csv_file = sys.argv[1]
    if not os.path.exists(csv_file):
        print(f"Fehler: Datei nicht gefunden: {csv_file}")
        sys.exit(1)
        
    generate_5_3_2_plots(csv_file)
