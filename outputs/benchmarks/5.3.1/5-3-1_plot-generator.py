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

    # ---------------------------------------------------------
    # GRAPH 2: VRAM Bandwidth Variance at 4K
    # ---------------------------------------------------------
    df_g2 = df[df['StepValue'] == 4.0].copy()
    
    if not df_g2.empty:
        fig, ax = plt.subplots(figsize=(10, 6))
        sns.boxplot(data=df_g2, x='Architecture', y='TotalGpuMs', hue='Architecture', 
                    palette=bu.ARCH_COLOR_MAP, legend=False, ax=ax, whis=(1, 99), 
                    boxprops=dict(alpha=0.5, edgecolor='black', linewidth=1.5),
                    flierprops=dict(marker='o', markersize=3, alpha=0.5, markerfacecolor='black', markeredgecolor='none'))
                    
        ax.set_xlabel('Architektur (bei 4K Auflösung)')
        ax.set_ylabel('GPU-Ausführungszeit (ms)')
        bu.apply_strict_styling(ax, force_zero_y=False)
        bu.annotate_boxplot(ax, df_g2, 'Architecture', 'TotalGpuMs', hue_col='Architecture')
        
        plt.tight_layout()
        plt.savefig(os.path.join(output_dir, '5_3_1_resolution_4k_variance.pdf'), format='pdf', bbox_inches='tight')
        plt.close()
        print("-> Graph 2 exportiert (5_3_1_resolution_4k_variance.pdf)")

    # ---------------------------------------------------------
    # GRAPH 3: Deferred Component Scaling (Grouped Stacked Bar)
    # ---------------------------------------------------------
    df_g3 = df[df['Architecture'].isin(['Deferred_Uber', 'Deferred_Volume'])].copy()
    
    if not df_g3.empty:
        df_g3_agg = df_g3.groupby(['Resolution', 'Architecture'], sort=False, observed=False)[['GeomMs', 'LightMs', 'TotalGpuMs']].mean().reset_index()
        
        fig, ax = plt.subplots(figsize=(12, 7))
        
        x = np.arange(len(resolution_order))
        width = 0.35
        
        color_uber_geom = '#6E49A8'       
        color_uber_light = '#9C77D6'      
        color_uber_overhead = '#C6A0F6'   
        
        color_vol_geom = '#4B9685'        
        color_vol_light = '#6BBFAD'       
        color_vol_overhead = '#8BD5CA'    
        
        uber_data = df_g3_agg[df_g3_agg['Architecture'] == 'Deferred_Uber'].set_index('Resolution').reindex(resolution_order)
        uber_data['OverheadMs'] = uber_data['TotalGpuMs'] - (uber_data['GeomMs'] + uber_data['LightMs'])
        
        vol_data = df_g3_agg[df_g3_agg['Architecture'] == 'Deferred_Volume'].set_index('Resolution').reindex(resolution_order)
        vol_data['OverheadMs'] = vol_data['TotalGpuMs'] - (vol_data['GeomMs'] + vol_data['LightMs'])
        
        # Uber
        ax.bar(x - width/2, uber_data['GeomMs'], width, label='GeomMs (Uber)', color=color_uber_geom, edgecolor='black', linewidth=0.5)
        ax.bar(x - width/2, uber_data['LightMs'], width, bottom=uber_data['GeomMs'], label='LightMs (Uber)', color=color_uber_light, edgecolor='black', linewidth=0.5)
        ax.bar(x - width/2, uber_data['OverheadMs'], width, bottom=uber_data['GeomMs'] + uber_data['LightMs'], label='OverheadMs (Uber)', color=color_uber_overhead, edgecolor='black', linewidth=0.5)
        
        # Volume
        ax.bar(x + width/2, vol_data['GeomMs'], width, label='GeomMs (Volume)', color=color_vol_geom, edgecolor='black', linewidth=0.5)
        ax.bar(x + width/2, vol_data['LightMs'], width, bottom=vol_data['GeomMs'], label='LightMs (Volume)', color=color_vol_light, edgecolor='black', linewidth=0.5)
        ax.bar(x + width/2, vol_data['OverheadMs'], width, bottom=vol_data['GeomMs'] + vol_data['LightMs'], label='OverheadMs (Volume)', color=color_vol_overhead, edgecolor='black', linewidth=0.5)
        
        # Data Ink: Stacked annotations
        for p in ax.patches:
            height = p.get_height()
            if height > 0.05:
                ax.text(p.get_x() + p.get_width() / 2., p.get_y() + height / 2.,
                        f'{height:.2f}', ha='center', va='center', fontsize=8, color='white' if height > 0.5 else 'black')
        
        ax.set_xticks(x)
        ax.set_xticklabels(resolution_order)
        
        ax.set_xlabel('Auflösung')
        ax.set_ylabel('GPU-Ausführungszeit (ms)')
        
        bu.apply_strict_styling(ax, force_zero_y=False)
        
        # Custom Legend
        from matplotlib.patches import Patch
        legend_elements = [
            Patch(facecolor=color_uber_geom, edgecolor='black', label='GeomMs (Uber)'),
            Patch(facecolor=color_uber_light, edgecolor='black', label='LightMs (Uber)'),
            Patch(facecolor=color_vol_geom, edgecolor='black', label='GeomMs (Volume)'),
            Patch(facecolor=color_vol_light, edgecolor='black', label='LightMs (Volume)')
        ]
        ax.legend(handles=legend_elements, title='Deferred Metriken', loc='upper left', framealpha=0.8, facecolor='white', edgecolor='gray')
        
        plt.tight_layout()
        plt.savefig(os.path.join(output_dir, '5_3_1_deferred_component_scaling.pdf'), format='pdf', bbox_inches='tight')
        plt.close()
        print("-> Graph 3 exportiert (5_3_1_deferred_component_scaling.pdf)")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python 5-3-1_plot-generator.py <path_to_csv>")
        sys.exit(1)
        
    csv_file = sys.argv[1]
    if not os.path.exists(csv_file):
        print(f"Fehler: Datei nicht gefunden: {csv_file}")
        sys.exit(1)
        
    generate_5_3_1_plots(csv_file)
