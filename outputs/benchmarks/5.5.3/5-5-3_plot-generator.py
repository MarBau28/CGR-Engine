import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import sys
import os

# Füge Root-Benchmark-Ordner zum Pfad hinzu für benchmark_utils
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))
import benchmark_utils as bu


def generate_5_5_3_plots(csv_path):
    output_dir = os.path.dirname(csv_path) or '.'

    print(f"Verarbeite Datensatz Kernel Bandwidth Limits: {csv_path}")
    df = pd.read_csv(csv_path)

    df = bu.clean_arch(df)

    # Filtere Forward komplett raus, da wir Kernel Bandwidth nur in Deferred evaluieren
    df = df[df['Architecture'].isin(['Deferred_Uber', 'Deferred_Volume'])].copy()

    # Mapping für StepValue (Kuwahara Radius) zu kategorialen Strings
    step_values_sorted = sorted(df['StepValue'].unique())
    step_labels = [str(int(val)) if val.is_integer() else str(val) for val in step_values_sorted]

    step_mapping = dict(zip(step_values_sorted, step_labels))
    df['Radius_Label'] = df['StepValue'].map(step_mapping)
    df['Radius_Label'] = pd.Categorical(df['Radius_Label'], categories=step_labels, ordered=True)

    # Ausreißer-Korrektur (3 Sigma)
    df = bu.filter_outliers_3sigma(df, ['Radius_Label', 'Architecture'], ['LightMs', 'FPS'])

    bu.setup_latex_font()

    # ---------------------------------------------------------
    # FIGURE 1: Line Chart (Kernel Bandwidth Scalability)
    # ---------------------------------------------------------
    df_g1 = df.groupby(['Radius_Label', 'Architecture'], sort=False, observed=False)['LightMs'].mean().reset_index()

    fig, ax = plt.subplots(figsize=(10, 6))

    arch_order_filtered = ['Deferred_Uber', 'Deferred_Volume']
    for arch in arch_order_filtered:
        arch_data = df_g1[df_g1['Architecture'] == arch].set_index('Radius_Label').reindex(step_labels)
        arch_data = arch_data.dropna(subset=['LightMs'])

        if not arch_data.empty:
            color = bu.ARCH_COLOR_MAP[arch]
            ax.plot(arch_data.index, arch_data['LightMs'],
                    color=color, linestyle='-', marker='o', linewidth=2, markersize=6,
                    label=arch)

    ax.set_xlabel('Kuwahara Radius')
    ax.set_ylabel('GPU Shading Zeit (ms)')

    bu.apply_strict_styling(ax, force_zero_y=True)
    ax.legend(title='Deferred-Architektur', loc='upper left', framealpha=0.8, facecolor='white', edgecolor='gray')

    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, '5_5_3_kernel_bandwidth_scalability.pdf'), format='pdf', bbox_inches='tight')
    plt.close()
    print("-> Graph 1 exportiert (5_5_3_kernel_bandwidth_scalability.pdf)")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python 5-5-3_plot-generator.py <path_to_csv>")
        sys.exit(1)

    csv_file = sys.argv[1]
    if not os.path.exists(csv_file):
        print(f"Fehler: Datei nicht gefunden: {csv_file}")
        sys.exit(1)

    generate_5_5_3_plots(csv_file)
