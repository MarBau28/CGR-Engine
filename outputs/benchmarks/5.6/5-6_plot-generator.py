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

    if not df1.empty and not df2.empty and not df3.empty:
        agg1 = df1.groupby('Architecture', sort=False, observed=False)['TotalGpuMs'].mean()
        agg2 = df2.groupby('Architecture', sort=False, observed=False)['TotalGpuMs'].mean()
        agg3 = df3.groupby('Architecture', sort=False, observed=False)['TotalGpuMs'].mean()

    # ---------------------------------------------------------
    # FIGURE 1: Line Chart (Flythrough Frame-Time Variance - GM)
    # ---------------------------------------------------------
    if not df1.empty:
        fig, ax = plt.subplots(figsize=(12, 6))
        for arch in bu.ARCH_ORDER:
            arch_data = df1[df1['Architecture'] == arch].sort_values('FrameNumber')
            if not arch_data.empty:
                color = bu.ARCH_COLOR_MAP[arch]
                ax.plot(arch_data['FrameNumber'], arch_data['TotalFrameMs'], color=color, alpha=0.3, linewidth=1.0)
                rolling = arch_data['TotalFrameMs'].rolling(window=90, center=True).mean()
                ax.plot(arch_data['FrameNumber'], rolling, color=color, linewidth=2.5, label=arch)

        ax.set_xlabel('Kamera-Flythrough (Frame-Index)')
        ax.set_ylabel('Frame-Zeit (ms)')
        bu.apply_strict_styling(ax, force_zero_y=True)
        ax.legend(title='Rendering-Pipeline', loc='upper left', framealpha=0.8, facecolor='white', edgecolor='gray')
        plt.tight_layout()
        plt.savefig(os.path.join(target_dir, '5_6_flythrough_variance_geom.pdf'), format='pdf', bbox_inches='tight')
        plt.close()
        print("-> Graph 1 exportiert (5_6_flythrough_variance_geom.pdf)")

    # ---------------------------------------------------------
    # FIGURE 2: Line Chart (Flythrough Frame-Time Variance - LIGHT)
    # ---------------------------------------------------------
    if not df2.empty:
        fig, ax = plt.subplots(figsize=(12, 6))
        for arch in bu.ARCH_ORDER:
            arch_data = df2[df2['Architecture'] == arch].sort_values('FrameNumber')
            if not arch_data.empty:
                color = bu.ARCH_COLOR_MAP[arch]
                ax.plot(arch_data['FrameNumber'], arch_data['TotalFrameMs'], color=color, alpha=0.3, linewidth=1.0)
                rolling = arch_data['TotalFrameMs'].rolling(window=90, center=True).mean()
                ax.plot(arch_data['FrameNumber'], rolling, color=color, linewidth=2.5, label=arch)

        ax.set_xlabel('Kamera-Flythrough (Frame-Index)')
        ax.set_ylabel('Frame-Zeit (ms)')
        bu.apply_strict_styling(ax, force_zero_y=True)
        ax.legend(title='Rendering-Pipeline', loc='upper left', framealpha=0.8, facecolor='white', edgecolor='gray')
        plt.tight_layout()
        plt.savefig(os.path.join(target_dir, '5_6_flythrough_variance_light.pdf'), format='pdf', bbox_inches='tight')
        plt.close()
        print("-> Graph 2 exportiert (5_6_flythrough_variance_light.pdf)")

    # ---------------------------------------------------------
    # FIGURE 3: Line Chart (Flythrough Frame-Time Variance - NPR)
    # ---------------------------------------------------------
    if not df3.empty:
        fig, ax = plt.subplots(figsize=(12, 6))
        for arch in bu.ARCH_ORDER:
            arch_data = df3[df3['Architecture'] == arch].sort_values('FrameNumber')
            if not arch_data.empty:
                color = bu.ARCH_COLOR_MAP[arch]
                ax.plot(arch_data['FrameNumber'], arch_data['TotalFrameMs'], color=color, alpha=0.3, linewidth=1.0)
                rolling = arch_data['TotalFrameMs'].rolling(window=90, center=True).mean()
                ax.plot(arch_data['FrameNumber'], rolling, color=color, linewidth=2.5, label=arch)

        ax.set_xlabel('Kamera-Flythrough (Frame-Index)')
        ax.set_ylabel('Frame-Zeit (ms)')
        bu.apply_strict_styling(ax, force_zero_y=True)
        ax.legend(title='Rendering-Pipeline', loc='upper left', framealpha=0.8, facecolor='white', edgecolor='gray')
        plt.tight_layout()
        plt.savefig(os.path.join(target_dir, '5_6_flythrough_variance_npr.pdf'), format='pdf', bbox_inches='tight')
        plt.close()
        print("-> Graph 3 exportiert (5_6_flythrough_variance_npr.pdf)")

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
