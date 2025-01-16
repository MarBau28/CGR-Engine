import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import sys
import os

# Füge Root-Benchmark-Ordner zum Pfad hinzu für benchmark_utils
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))
import benchmark_utils as bu

def format_triangles(val):
    mapping = {
        24000: '24K',
        256000: '256K',
        1024000: '1.02M',
        4096000: '4.09M',
        16384000: '16.38M'
    }
    return mapping.get(val, str(val))

def generate_5_2_1_plots(csv_path):
    output_dir = os.path.dirname(csv_path) or '.'
    
    print(f"Verarbeite Datensatz: {csv_path}")
    df = pd.read_csv(csv_path)
    
    # 1. Daten sortieren und Kategorien formatieren
    df = df.sort_values(by='Triangles', ascending=True)
    df['Triangles_Label'] = df['Triangles'].apply(format_triangles)
    
    # Architektur Standardisierung
    df = bu.clean_arch(df)
    
    # Ausreißer-Korrektur (3 Sigma) auf Ebene Triangles & Architektur
    df = bu.filter_outliers_3sigma(df, ['Triangles_Label', 'Architecture'], ['TotalGpuMs', 'CpuRenderMs'])
    
    # Explizite Sortierung der X-Achse
    triangle_order = ['24K', '256K', '1.02M', '4.09M', '16.38M']
    actual_archs = df['Architecture'].unique()
    
    bu.setup_latex_font()

    # ---------------------------------------------------------
    # GRAPH 2: CPU vs. GPU Bottleneck Divergence
    # ---------------------------------------------------------
    df_g2 = df.groupby(['Triangles_Label', 'Architecture'], sort=False, observed=False).agg({
        'TotalGpuMs': 'mean',
        'CpuRenderMs': 'mean'
    }).reset_index()

    fig, ax = plt.subplots(figsize=(10, 6))
    
    for arch in actual_archs:
        arch_data = df_g2[df_g2['Architecture'] == arch].set_index('Triangles_Label').reindex(triangle_order)
        color = bu.ARCH_COLOR_MAP.get(arch, '#000000') 
        
        # GPU (Durchgezogene Linie, kreisförmige Marker)
        ax.plot(arch_data.index, arch_data['TotalGpuMs'], 
                color=color, linestyle='-', marker='o', linewidth=2.5, 
                label=f'{arch} (GPU-Gesamt)')
        
        # CPU (Gestrichelte Linie, dreieckige Marker)
        ax.plot(arch_data.index, arch_data['CpuRenderMs'], 
                color=color, linestyle='--', marker='^', linewidth=2.5, 
                label=f'{arch} (CPU-Gesamt)')

    ax.set_xlabel('Aktive Dreiecke (Active Triangles)')
    ax.set_ylabel('Ausführungszeit (ms)')
    bu.apply_strict_styling(ax, force_zero_y=True)
    
    # Legende nach oben links innerhalb des Graphen verschoben
    ax.legend(title='Komponentenkosten', loc='upper left')
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, '5-2-1_bottleneck_divergence.pdf'), format='pdf', bbox_inches='tight')
    plt.close()
    
    print(f"Diagramme erfolgreich generiert. Pfad: {os.path.abspath(output_dir)}")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python 5-2-1_plot-generator.py <path_to_csv>")
        sys.exit(1)
        
    csv_file = sys.argv[1]
    if not os.path.exists(csv_file):
        print(f"Fehler: Datei nicht gefunden: {csv_file}")
        sys.exit(1)
        
    generate_5_2_1_plots(csv_file)
