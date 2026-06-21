import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import numpy as np
import sys
import os
import glob

# Füge Root-Benchmark-Ordner zum Pfad hinzu für benchmark_utils
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))
import benchmark_utils as bu

def load_pass_data(directory, pass_str):
    search_pattern = os.path.join(directory, f"*{pass_str}*.csv")
    files = glob.glob(search_pattern)
    if not files:
        print(f"Warnung: Keine CSV für {pass_str} in {directory} gefunden.")
        return pd.DataFrame()
    return pd.read_csv(files[0])

def generate_5_6_plots(target_dir):
    print(f"Verarbeite Datensatz Multiple Passes (5.6) in: {target_dir}")
    
    df1 = load_pass_data(target_dir, "Pass1")
    df2 = load_pass_data(target_dir, "Pass2")
    df3 = load_pass_data(target_dir, "Pass3")
    df4 = load_pass_data(target_dir, "Pass4")
    
    # ---------------------------------------------------------
    # GLOBALE ORDNUNG UND FARBEN
    # ---------------------------------------------------------
    df1 = bu.clean_arch(df1)
    df2 = bu.clean_arch(df2)
    df3 = bu.clean_arch(df3)
    df4 = bu.clean_arch(df4)

    # Ausreißer-Korrektur (3 Sigma)
    if not df1.empty: df1 = bu.filter_outliers_3sigma(df1, ['Architecture'], ['TotalGpuMs', 'TotalFrameMs'])
    if not df2.empty: df2 = bu.filter_outliers_3sigma(df2, ['Architecture'], ['TotalGpuMs', 'TotalFrameMs'])
    if not df3.empty: df3 = bu.filter_outliers_3sigma(df3, ['Architecture'], ['TotalGpuMs', 'TotalFrameMs'])
    if not df4.empty: df4 = bu.filter_outliers_3sigma(df4, ['Architecture'], ['TotalGpuMs', 'TotalFrameMs'])

    bu.setup_latex_font()

    # ---------------------------------------------------------
    # FIGURE 1: Dual Bar Chart (Decoupled Geometry & Architectural Tax)
    # ---------------------------------------------------------
    if not df1.empty and not df2.empty and not df3.empty:
        agg1 = df1.groupby('Architecture', sort=False, observed=False)['TotalGpuMs'].mean()
        agg2 = df2.groupby('Architecture', sort=False, observed=False)['TotalGpuMs'].mean()
        agg3 = df3.groupby('Architecture', sort=False, observed=False)['TotalGpuMs'].mean()
        
        fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 6))
        
        x = np.arange(len(bu.ARCH_ORDER))
        width = 0.5
        
        for i, arch in enumerate(bu.ARCH_ORDER):
            if arch in agg1 and arch in agg2 and arch in agg3:
                base_val = agg1[arch]
                v2 = agg2[arch]
                v3 = agg3[arch]
                
                stack1_val = max(0, v2 - base_val)
                stack2_val = max(0, v3 - v2)
                
                base_color = bu.ARCH_COLOR_MAP[arch]
                
                # Subplot A: Geometry Baseline (Left)
                ax1.bar(x[i], base_val, width, color=base_color, alpha=0.4, edgecolor='black', linewidth=1.0)
                if base_val > 0:
                    ax1.text(x[i], base_val + agg1.max()*0.01, f"{base_val:.2f}", ha='center', va='bottom', fontsize=10)
                
                # Subplot B: The Shading Tax (Right) - No Base!
                ax2.bar(x[i], stack1_val, width, color=base_color, alpha=0.7, edgecolor='black', linewidth=1.0)
                ax2.bar(x[i], stack2_val, width, bottom=stack1_val, color=base_color, alpha=1.0, hatch='//', edgecolor='black', linewidth=1.0)
                
                # Werte neben die einzelnen Stack-Portionen schreiben
                if stack1_val > 0.01:
                    ax2.text(x[i] + width/2 + 0.03, stack1_val/2, f"{stack1_val:.2f}", ha='left', va='center', fontsize=10)
                if stack2_val > 0.01:
                    ax2.text(x[i] + width/2 + 0.03, stack1_val + stack2_val/2, f"{stack2_val:.2f}", ha='left', va='center', fontsize=10)

        for ax in [ax1, ax2]:
            ax.set_xticks(x)
            ax.set_xticklabels(bu.ARCH_ORDER)
            ax.set_xlabel('Rendering-Pipeline')
            bu.apply_strict_styling(ax, force_zero_y=True)
            
        ax1.set_ylabel('Basis GPU-Zeit (ms)')
        ax2.set_ylabel('Zusätzliche GPU-Zeit (ms)')
        
        from matplotlib.patches import Patch
        legend_elements = [
            Patch(facecolor='gray', alpha=0.4, edgecolor='black', label='Basis: Geometrie (Pass 1)'),
            Patch(facecolor='gray', alpha=0.7, edgecolor='black', label='Zusatzlast: Beleuchtung (Pass 2)'),
            Patch(facecolor='gray', alpha=1.0, hatch='//', edgecolor='black', label='Zusatzlast: Stil-Entropie (Pass 3)')
        ]
        ax2.legend(handles=legend_elements, loc='upper right', framealpha=0.8, facecolor='white', edgecolor='gray')
        
        plt.tight_layout()
        plt.savefig(os.path.join(target_dir, '5_6_cumulative_rendering_cost.pdf'), format='pdf', bbox_inches='tight')
        plt.close()
        print("-> Graph 1 exportiert (5_6_cumulative_rendering_cost.pdf)")

    # ---------------------------------------------------------
    # FIGURE 2: Line Chart (Flythrough Frame-Time Variance)
    # ---------------------------------------------------------
    if not df3.empty:
        fig, ax = plt.subplots(figsize=(12, 6))
        
        for arch in bu.ARCH_ORDER:
            arch_data = df3[df3['Architecture'] == arch].sort_values('FrameNumber')
            if not arch_data.empty:
                color = bu.ARCH_COLOR_MAP[arch]
                
                # Raw Data (Opacity 0.5)
                ax.plot(arch_data['FrameNumber'], arch_data['TotalFrameMs'], 
                        color=color, alpha=0.3, linewidth=1.0)
                
                # Rolling Average (Solid Line) - Angepasst auf 3000 Frames -> größeres Window (z.B. 90) für dieselbe relative Glättung
                rolling = arch_data['TotalFrameMs'].rolling(window=90, center=True).mean()
                ax.plot(arch_data['FrameNumber'], rolling, 
                        color=color, linewidth=2.5, label=arch)

        ax.set_xlabel('Kamera-Flythrough (Frame-Index)')
        ax.set_ylabel('Frame-Zeit (ms)')
        
        bu.apply_strict_styling(ax, force_zero_y=True)
        ax.legend(title='Rendering-Pipeline', loc='upper right', framealpha=0.8, facecolor='white', edgecolor='gray')
        
        plt.tight_layout()
        plt.savefig(os.path.join(target_dir, '5_6_flythrough_variance.pdf'), format='pdf', bbox_inches='tight')
        plt.close()
        print("-> Graph 2 exportiert (5_6_flythrough_variance.pdf)")

    # ---------------------------------------------------------
    # FIGURE 3: Box Plot (Deferred Max Fidelity - Pass 4)
    # ---------------------------------------------------------
    if not df4.empty:
        df4_deferred = df4[df4['Architecture'] != 'Forward'].copy()
        
        # Sicherstellen dass ungenutzte Kategorien entfernt werden
        if hasattr(df4_deferred['Architecture'], 'cat'):
            df4_deferred['Architecture'] = df4_deferred['Architecture'].cat.remove_unused_categories()
        
        if not df4_deferred.empty:
            fig, ax = plt.subplots(figsize=(8, 6))
            
            sns.boxplot(data=df4_deferred, x='Architecture', y='TotalFrameMs', hue='Architecture', 
                        palette=bu.ARCH_COLOR_MAP, ax=ax, whis=(1, 99), dodge=False,
                        boxprops=dict(alpha=0.5), 
                        flierprops=dict(marker='o', markersize=2, alpha=0.3, markerfacecolor='black', markeredgecolor='none'))
            
            if ax.legend_ is not None:
                ax.legend_.remove()
                
            import matplotlib.patches
            boxes = [patch for patch in ax.patches if type(patch) == matplotlib.patches.PathPatch]
            
            plotted_archs = df4_deferred['Architecture'].unique()
            box_idx = 0
            for arch in ['Deferred_Uber', 'Deferred_Volume']:
                if arch in plotted_archs.tolist():
                    if box_idx < len(boxes):
                        box = boxes[box_idx]
                        box.set_edgecolor(bu.ARCH_COLOR_MAP[arch])
                        box.set_linewidth(1.5)
                        box_idx += 1

            ax.set_xlabel('Deferred-Architektur')
            ax.set_ylabel('Gesamte Frame-Zeit (ms)')
            
            bu.apply_strict_styling(ax, force_zero_y=False) # Auto scale für Boxplot Limits
            bu.annotate_boxplot(ax, df4_deferred, 'Architecture', 'TotalFrameMs', hue_col='Architecture')
            
            plt.tight_layout()
            plt.savefig(os.path.join(target_dir, '5_6_deferred_max_fidelity.pdf'), format='pdf', bbox_inches='tight')
            plt.close()
            print("-> Graph 3 exportiert (5_6_deferred_max_fidelity.pdf)")

    # ---------------------------------------------------------
    # FIGURE 4: Combined Stacked Bar Chart (Cumulative GPU Tax)
    # ---------------------------------------------------------
    if not df1.empty and not df2.empty and not df3.empty:
        fig, ax = plt.subplots(figsize=(10, 6))
        
        x = np.arange(len(bu.ARCH_ORDER))
        width = 0.5
        
        for i, arch in enumerate(bu.ARCH_ORDER):
            if arch in agg1 and arch in agg2 and arch in agg3:
                base_val = agg1[arch]
                v2 = agg2[arch]
                v3 = agg3[arch]
                
                stack1_val = max(0, v2 - base_val)
                stack2_val = max(0, v3 - v2)
                
                base_color = bu.ARCH_COLOR_MAP[arch]
                
                # Stack 0: Geometry Base
                ax.bar(x[i], base_val, width, color=base_color, alpha=0.4, edgecolor='black', linewidth=1.0)
                # Stack 1: Shading Base
                ax.bar(x[i], stack1_val, width, bottom=base_val, color=base_color, alpha=0.7, edgecolor='black', linewidth=1.0)
                # Stack 2: Style Entropy
                ax.bar(x[i], stack2_val, width, bottom=base_val+stack1_val, color=base_color, alpha=1.0, hatch='//', edgecolor='black', linewidth=1.0)
                
                # Werte neben die Portionen schreiben
                if base_val > 0.01:
                    ax.text(x[i] + width/2 + 0.03, base_val/2, f"{base_val:.2f}", ha='left', va='center', fontsize=10)
                if stack1_val > 0.01:
                    ax.text(x[i] + width/2 + 0.03, base_val + stack1_val/2, f"{stack1_val:.2f}", ha='left', va='center', fontsize=10)
                if stack2_val > 0.01:
                    ax.text(x[i] + width/2 + 0.03, base_val + stack1_val + stack2_val/2, f"{stack2_val:.2f}", ha='left', va='center', fontsize=10)

        ax.set_xticks(x)
        ax.set_xticklabels(bu.ARCH_ORDER)
        ax.set_xlabel('Rendering-Pipeline')
        ax.set_ylabel('GPU Ausführungszeit (ms)')
        
        bu.apply_strict_styling(ax, force_zero_y=True)
        
        from matplotlib.patches import Patch
        legend_elements = [
            Patch(facecolor='gray', alpha=0.4, edgecolor='black', label='Basis: Geometrie (Pass 1)'),
            Patch(facecolor='gray', alpha=0.7, edgecolor='black', label='Zusatzlast: Beleuchtung (Pass 2)'),
            Patch(facecolor='gray', alpha=1.0, hatch='//', edgecolor='black', label='Zusatzlast: Stil-Entropie (Pass 3)')
        ]
        # Legende innerhalb des Graphen auf der rechten Seite
        ax.legend(handles=legend_elements, loc='upper right', framealpha=0.8, facecolor='white', edgecolor='gray')
        
        plt.tight_layout()
        plt.savefig(os.path.join(target_dir, '5_6_cumulative_rendering_cost_combined.pdf'), format='pdf', bbox_inches='tight')
        plt.close()
        print("-> Graph 4 exportiert (5_6_cumulative_rendering_cost_combined.pdf)")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python 5-6_plot-generator.py <directory_path>")
        sys.exit(1)
        
    target_directory = sys.argv[1]
    if not os.path.exists(target_directory):
        print(f"Fehler: Ordner nicht gefunden: {target_directory}")
        sys.exit(1)
        
    generate_5_6_plots(target_directory)
