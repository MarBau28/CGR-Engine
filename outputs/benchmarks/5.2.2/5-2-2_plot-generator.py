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
    
    # Da dies ein Liniendiagramm ist, macht annotate_bar_chart hier keinen Sinn.
    
    # 2 separate Legenden zur sauberen Lesbarkeit generieren
    legend_elements_arch = [Line2D([0], [0], color=bu.ARCH_COLOR_MAP.get(a, '#000000'), lw=3, label=a) for a in actual_archs]
    legend_elements_metric = [
        Line2D([0], [0], color='black', lw=2, linestyle='-', marker='o', label='CpuLogicMs'),
        Line2D([0], [0], color='black', lw=2, linestyle='--', marker='^', label='CpuRenderMs')
    ]
    
    leg_arch = ax.legend(handles=legend_elements_arch, title='Architektur', loc='upper left', framealpha=0.8, facecolor='white', edgecolor='gray')
    ax.add_artist(leg_arch)
    ax.legend(handles=legend_elements_metric, title='Metrik', loc='upper right', framealpha=0.8, facecolor='white', edgecolor='gray')
    
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, '5_2_2_cpu_scaling_logic_vs_render.pdf'), format='pdf', bbox_inches='tight')
    plt.close()
    print("-> Graph 1 exportiert (5_2_2_cpu_scaling_logic_vs_render.pdf)")

    # ---------------------------------------------------------
    # GRAPH 2: Frame Pacing at 50,000 Instances
    # ---------------------------------------------------------
    df_g2 = df[df['Instances'] == 50000.0].copy()
    
    if not df_g2.empty:
        fig, ax = plt.subplots(figsize=(10, 6))
        
        sns.boxplot(data=df_g2, x='Architecture', y='TotalFrameMs', hue='Architecture', 
                    palette=bu.ARCH_COLOR_MAP, legend=False, ax=ax, whis=(1, 99), 
                    boxprops=dict(alpha=0.5, edgecolor='black', linewidth=1.5),
                    flierprops=dict(marker='o', markersize=3, alpha=0.5, markerfacecolor='black', markeredgecolor='none'))
                    
        ax.set_xlabel('Architektur (bei 50.000 Instanzen)')
        ax.set_ylabel('Gesamte Frame-Zeit (ms)')
        bu.apply_strict_styling(ax, force_zero_y=False)
        bu.annotate_boxplot(ax, df_g2, 'Architecture', 'TotalFrameMs', hue_col='Architecture')
        
        plt.tight_layout()
        plt.savefig(os.path.join(output_dir, '5_2_2_cpu_stutter_variance.pdf'), format='pdf', bbox_inches='tight')
        plt.close()
        print("-> Graph 2 exportiert (5_2_2_cpu_stutter_variance.pdf)")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python 5-2-2_plot-generator.py <path_to_csv>")
        sys.exit(1)
        
    csv_file = sys.argv[1]
    if not os.path.exists(csv_file):
        print(f"Fehler: Datei nicht gefunden: {csv_file}")
        sys.exit(1)
        
    generate_5_2_2_plots(csv_file)
