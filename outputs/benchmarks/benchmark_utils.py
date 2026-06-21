import pandas as pd
import numpy as np
import matplotlib.pyplot as plt

# ---------------------------------------------------------
# GLOBALE ORDNUNG UND FARBEN
# ---------------------------------------------------------
ARCH_ORDER = ['Forward', 'Deferred_Uber', 'Deferred_Volume']
ARCH_COLOR_MAP = {
    'Forward': '#7DC4E4',
    'Deferred_Uber': '#C6A0F6',
    'Deferred_Volume': '#8BD5CA'
}

def clean_arch(df):
    """
    Standardisiert die Architekturnamen und setzt die Categorical Ordnung.
    """
    if not df.empty and 'Architecture' in df.columns:
        df['Architecture'] = df['Architecture'].replace({
            'DeferredUber': 'Deferred_Uber', 
            'DeferredVolume': 'Deferred_Volume'
        })
        df['Architecture'] = pd.Categorical(df['Architecture'], categories=ARCH_ORDER, ordered=True)
    return df

# ---------------------------------------------------------
# WISSENSCHAFTLICHE EMPIRIE (3-SIGMA)
# ---------------------------------------------------------
def filter_outliers_3sigma(df, group_cols, target_cols):
    """
    Filtert statistische Ausreißer (z.B. OS Interrupts) basierend auf dem 3-Sigma Prinzip.
    Dies stellt sicher, dass die Mean-Berechnung rein algorithmisch bleibt.
    """
    if df.empty: return df
    
    if isinstance(target_cols, str):
        target_cols = [target_cols]
        
    df_clean = df.copy()
    
    for tcol in target_cols:
        if tcol not in df_clean.columns: continue
        
        # Berechne Mean und Std per Gruppe
        stats = df_clean.groupby(group_cols, observed=False)[tcol].agg(['mean', 'std']).reset_index()
        
        # Merge stats zurück zum Original-DF
        df_clean = df_clean.merge(stats, on=group_cols, how='left')
        
        # Fill NaN std mit 0 (falls nur 1 Wert in der Gruppe existiert)
        df_clean['std'] = df_clean['std'].fillna(0)
        
        # 3 Sigma Grenzen
        lower_bound = df_clean['mean'] - 3 * df_clean['std']
        upper_bound = df_clean['mean'] + 3 * df_clean['std']
        
        # Filter mask
        mask = (df_clean[tcol] >= lower_bound) & (df_clean[tcol] <= upper_bound)
        df_clean = df_clean[mask]
        
        df_clean = df_clean.drop(columns=['mean', 'std'])
        
    return df_clean

# ---------------------------------------------------------
# VISUELLE KONSISTENZ & DATA INK
# ---------------------------------------------------------
def setup_latex_font():
    plt.rcParams['font.family'] = 'serif'
    plt.rcParams['font.size'] = 12
    plt.rcParams['axes.facecolor'] = 'white'
    plt.rcParams['figure.facecolor'] = 'white'

def apply_strict_styling(ax, force_zero_y=True):
    """
    Erzwingt das einheitliche LaTeX-Styling für jeden Graphen.
    """
    ax.set_title("") 
    ax.set_axisbelow(True) 
    ax.grid(True, which='both', axis='both', linestyle='--', alpha=0.3, color='#b0b0b0')
    ax.spines['top'].set_visible(False)
    ax.spines['right'].set_visible(False)
    if force_zero_y:
        ax.set_ylim(bottom=0.0)

def annotate_bar_chart(ax, fontsize=10, rotation=0):
    """
    Fügt exakte Metriken über die Balken eines einfachen Bar Charts ein (Data Ink).
    """
    if not ax.patches: return
    
    # Ermittle den Maximalwert, um den relativen Offset zu berechnen
    heights = [p.get_height() for p in ax.patches]
    if not heights: return
    
    max_y = max(heights)
    offset = max_y * 0.01
    
    for p in ax.patches:
        height = p.get_height()
        if height > 0.01:
            ax.text(p.get_x() + p.get_width() / 2., height + offset,
                    f'{height:.2f}', ha='center', va='bottom', fontsize=fontsize, rotation=rotation)

def annotate_boxplot(ax, df, x_col, y_col, hue_col=None, fontsize=10):
    """
    Annotiert die empirischen Median-Werte direkt auf die Achsen des Boxplots.
    """
    # Da Seaborn die Positionierung dynamisch vornimmt, extrahieren wir die X-Koordinaten der Ticks
    xticks = ax.get_xticks()
    xticklabels = [t.get_text() for t in ax.get_xticklabels()]
    
    # Berechne die Mediane aus dem (bereits gefilterten) DataFrame
    if hue_col:
        hues = df[hue_col].cat.categories if pd.api.types.is_categorical_dtype(df[hue_col]) else df[hue_col].unique()
        # Anzahl Hues bestimmt den Offset auf der X-Achse
        hue_count = len(hues)
        width = 0.8 / hue_count # default seaborn width
        
        for i, xtick in enumerate(xticks):
            x_val = xticklabels[i]
            for j, hue_val in enumerate(hues):
                subset = df[(df[x_col].astype(str) == x_val) & (df[hue_col] == hue_val)]
                if not subset.empty:
                    med = subset[y_col].median()
                    offset_x = xtick - 0.4 + (j + 0.5) * width
                    ax.text(offset_x, med, f'{med:.2f}', ha='center', va='center', 
                            fontsize=fontsize, color='black', 
                            bbox=dict(facecolor='white', alpha=0.7, edgecolor='none', pad=1))
    else:
        for i, xtick in enumerate(xticks):
            x_val = xticklabels[i]
            subset = df[df[x_col].astype(str) == x_val]
            if not subset.empty:
                med = subset[y_col].median()
                ax.text(xtick, med, f'{med:.2f}', ha='center', va='center', 
                        fontsize=fontsize, color='black', 
                        bbox=dict(facecolor='white', alpha=0.7, edgecolor='none', pad=1))
