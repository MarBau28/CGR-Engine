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

def generate_5_4_1_plots(csv_path):
    output_dir = os.path.dirname(csv_path) or '.'
    
    print(f"Verarbeite Datensatz 5.4.1: {csv_path}")
    df = pd.read_csv(csv_path)
    
    df = bu.clean_arch(df)
    
    # X-Achsen Werte als diskrete, string-basierte Kategorien behandeln
    step_values_sorted = sorted(df['StepValue'].unique())
    step_labels = [str(int(val)) if val.is_integer() else str(val) for val in step_values_sorted]
    
    # Mappe StepValue zu String-Labels im Dataframe
    step_mapping = dict(zip(step_values_sorted, step_labels))
    df['LightCount_Label'] = df['StepValue'].map(step_mapping)
    df['LightCount_Label'] = pd.Categorical(df['LightCount_Label'], categories=step_labels, ordered=True)

    # Ausreißer-Korrektur (3 Sigma)
    df = bu.filter_outliers_3sigma(df, ['LightCount_Label', 'Architecture'], ['TotalGpuMs'])
    
    bu.setup_latex_font()

    # ---------------------------------------------------------
    # FIGURE 1: Line Chart (Macro-Trend / Means)
    # ---------------------------------------------------------
    df_g1 = df.groupby(['LightCount_Label', 'Architecture'], sort=False, observed=False)['TotalGpuMs'].mean().reset_index()
    
    fig, ax = plt.subplots(figsize=(10, 6))
    
    for arch in bu.ARCH_ORDER:
        arch_data = df_g1[df_g1['Architecture'] == arch].set_index('LightCount_Label').reindex(step_labels)
        # Dropna() stellt sicher, dass Matplotlib die Linie einfach an der letzten validen Messung (500) stoppt.
        arch_data = arch_data.dropna(subset=['TotalGpuMs'])
        
        if not arch_data.empty:
            color = bu.ARCH_COLOR_MAP[arch]
            ax.plot(arch_data.index, arch_data['TotalGpuMs'], 
                    color=color, linestyle='-', marker='o', linewidth=2, markersize=6, 
                    label=arch)
            
    ax.set_xlabel('Anzahl aktiver Lichtquellen')
    ax.set_ylabel('GPU-Ausführungszeit (ms)')
    
    bu.apply_strict_styling(ax, force_zero_y=True)
    ax.legend(title='Architektur', loc='upper left', framealpha=0.8, facecolor='white', edgecolor='gray')
    
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, '5-4-1_LightCount_Means.pdf'), format='pdf', bbox_inches='tight')
    plt.close()
    print("-> Graph 1 exportiert (5-4-1_LightCount_Means.pdf)")

    # ---------------------------------------------------------
    # FIGURE 2: Grouped Box Plot (Micro-Trend / Variance)
    # ---------------------------------------------------------
    fig, ax = plt.subplots(figsize=(12, 6))
    
    if not df.empty:
        sns.boxplot(data=df, x='LightCount_Label', y='TotalGpuMs', hue='Architecture', 
                    palette=bu.ARCH_COLOR_MAP, ax=ax, whis=(1, 99), 
                    boxprops=dict(alpha=0.5), 
                    flierprops=dict(marker='o', markersize=2, alpha=0.3, markerfacecolor='black', markeredgecolor='none'))
                    
        boxes = [patch for patch in ax.patches if type(patch) == matplotlib.patches.PathPatch]
        
        num_x = len(step_labels)
        
        if len(boxes) > 0:
            for hue_idx, arch in enumerate(bu.ARCH_ORDER):
                base_color = bu.ARCH_COLOR_MAP[arch]
                for x_idx in range(num_x):
                    box_idx = hue_idx * num_x + x_idx
                    if box_idx < len(boxes):
                        box = boxes[box_idx]
                        box.set_edgecolor(base_color)
                        box.set_linewidth(1.5)

        ax.set_xlabel('Anzahl aktiver Lichtquellen')
        ax.set_ylabel('GPU-Ausführungszeit Verteilung (ms)')
        
        bu.apply_strict_styling(ax, force_zero_y=False)
        bu.annotate_boxplot(ax, df, 'LightCount_Label', 'TotalGpuMs', hue_col='Architecture')
        
        ax.legend(title='Architektur', loc='upper left', framealpha=0.8, facecolor='white', edgecolor='gray')
        
        plt.tight_layout()
        plt.savefig(os.path.join(output_dir, '5-4-1_LightCount_Variance.pdf'), format='pdf', bbox_inches='tight')
        plt.close()
        print("-> Graph 2 exportiert (5-4-1_LightCount_Variance.pdf)")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python 5-4-1_plot-generator.py <path_to_csv>")
        sys.exit(1)
        
    csv_file = sys.argv[1]
    if not os.path.exists(csv_file):
        print(f"Fehler: Datei nicht gefunden: {csv_file}")
        sys.exit(1)
        
    generate_5_4_1_plots(csv_file)
