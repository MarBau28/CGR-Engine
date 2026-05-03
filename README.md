## HyDra: Hybride Deferred-Rendering-Architektur

**Entwurf und Evaluierung einer hybriden Deferred-Rendering-Architektur zur selektiven, objektbasierten Stilisierung
komplexer 3D-Szenen**

Dieses Repository enthält den C++/OpenGL-Prototypen für die Bachelorarbeit von Marvin Baumann im Studiengang
Medieninformatik (Betreuer: Prof. Dr.-Ing. Gregor Lux).

---

### Motivation und Problemstellung

Die stilisierte Darstellung (**Non-Photorealistic Rendering**, NPR) ist in der Computergrafik essenziell für
künstlerische Abstraktion und technische Lesbarkeit. Gängige Engines reduzieren NPR oft auf globale
Post-Processing-Effekte, da semantische Informationen (wie Objekt-Zugehörigkeit) nach der Rasterisierung in
Standard-Pipelines verloren gehen.

- **Das Problem:** Selektive Stilisierung (z. B. Bauteil A als Gooch-Modell, Bauteil B mit Toon-Shading) erzwingt in
  Forward-Pipelines oft teure State-Changes oder führt bei hoher Lichtdichte und Überlappung zu massiven
  Performance-Einbußen.
- **Der Ansatz:** Ein **semantischer G-Buffer**, der Style-Metadaten speichert, entkoppelt die Shading-Komplexität von
  der Geometrie und ermöglicht eine hybride Koexistenz verschiedener NPR-Stile in einer stabilen Deferred-Architektur.

### Aktueller Implementierungsstand

Die **Kern-Pipeline der Engine ist vollständig implementiert** und funktionsfähig. Der Prototyp demonstriert erfolgreich
die Entkopplung von Geometrie und Stilisierung.

#### 1. Architektur & Pipeline (Abgeschlossen)

- **Geometry Pass:** Befüllung eines erweiterten G-Buffers (Albedo, Normalen, Depth) inklusive einer dedizierten *
  *Style-ID** pro Fragment.
- **Deferred Uber-Shader:** Ein zentraler Lighting-Pass, der basierend auf der Style-ID dynamisch zwischen verschiedenen
  NPR-Algorithmen (Blinn-Phong, Gooch, Toon) verzweigt.
- **Deferred Light Volumes:** Alternative Architektur zur performanten Akkumulation hunderter Lichtquellen mittels
  instanziierter Licht-Geometrie.
- **NPR-Integration:** Vollständige Unterstützung für **Kuwahara-Filter**, **Gooch-Shading**, **Toon-Shading** und *
  *Inverted-Hull Outlines**.

#### 2. Benchmarking & Telemetrie (Erster Entwurf / In Arbeit)

Das Framework zur quantitativen Analyse befindet sich derzeit in der **ersten Entwurfsphase**. Es dient dazu, die
Skalierbarkeit der Engine unter verschiedenen Stressfaktoren zu beweisen.

- **Automatisierte State-Machine:** Ein erster Prototyp steuert Testläufe (Suites A-H) automatisch und variiert
  Parameter wie Instanz-Anzahl, Licht-Topologie und Overdraw-Faktoren.
- **Hardware-Profiling:** Integration von asynchronen GPU-Timern (`GL_TIME_ELAPSED`) und CPU-Profilern zur isolierten
  Messung von Render-Passes.
- **CSV-Export:** Telemetrie-Daten werden für die wissenschaftliche Auswertung direkt in strukturierte CSV-Dateien
  geschrieben.

> **Hinweis:** Der Fokus liegt aktuell auf der Verfeinerung der einzelnen Test-Szenarien und der Stabilisierung der
> Messdaten unter extremen Bedingungen (z. B. maximale Tiefenkomplexität).

---

### Bedienung & Dashboard

Die Engine verfügt über ein integriertes **Echtzeit-Dashboard**, um die Auswirkungen der Architektur-Wechsel unmittelbar
zu visualisieren:

- **[TAB]**: Umschalten zwischen Forward-, Deferred-Uber- und Deferred-Volume-Architektur.
- **[R]**: Wechsel in den "NPR-Room" (steriler Test-Raum für Beleuchtungsstudien).
- **[F12]**: Startet den automatisierten Benchmarking-Prozess (Vorschau-Status).
- **[O, K, G, T]**: Manuelles Zu- oder Abschalten der NPR-Effekte (Outlines, Kuwahara, Gooch, Toon).

### Geplante Erweiterungen (Next Steps)

- **Refinement der Test-Suites:** Präzisere Isolierung von Vertex- vs. Fragment-Bottlenecks.
- **Optimierung der State-Machine:** Stabilisierung der Hardware-Clocks während der Messphasen (Warm-up Phasen).
- **Wissenschaftliche Auswertung:** Finalisierung der Vergleichsmetriken zwischen naiver Forward-Lösung und der hybriden
  Deferred-Struktur.

---

#### Tech Stack

- **Sprache:** C++20
- **Graphics API:** OpenGL 3.3+ (Core Profile)
- **Libraries:** raylib (Windowing & Math), glad (Extension Loading)

---

#### Zusammenfassung

- **Status Architektur:** Abgeschlossen und funktional.
- **Status Benchmarking:** Erster Prototyp implementiert; Fokus liegt nun auf der Verfeinerung der Test-Methodik.
- **Ziel:** Nachweis der algorithmischen Überlegenheit der hybriden Struktur bei hoher Szenenkomplexität.