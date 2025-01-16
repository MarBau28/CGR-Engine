## HyDra: Hybride Deferred-Rendering-Architektur

**Entwurf und Evaluierung einer hybriden Deferred-Rendering-Architektur zur selektiven, objektbasierten Stilisierung
komplexer 3D-Szenen**

Dieses Repository enthält den C++/OpenGL-Prototypen für die Bachelorarbeit von Marvin Baumann im Studiengang
Medieninformatik.

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

---

### Tech Stack

- **Sprache:** C++23
- **Graphics API:** OpenGL 3.3+ (Core Profile)
- **Libraries:** GLFW (Windowing & Input), glad (Extension Loading), glm (Math), raylib, stb
- **Build:** CMake ≥ 3.21 (Presets), vcpkg im Manifest-Modus

---

### Build

Alle Abhängigkeiten installiert vcpkg **automatisch beim ersten CMake-Configure** (Manifest-Modus, `vcpkg.json`).
Es gibt keinen manuellen `vcpkg install`-Schritt.

#### Einmalige Einrichtung

| Plattform   | Schritte                                                                                                                                                                                                                                      |
| ----------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Windows** | 1. Visual Studio 2022+ mit C++-Workload installieren.<br>2. Umgebungsvariable `VCPKG_ROOT` auf das VS-gebündelte vcpkg setzen, z. B. `C:\Program Files\Microsoft Visual Studio\18\Community\VC\vcpkg`.<br>3. Terminal/IDE danach neu starten. |
| **Linux**   | 1. GCC/Clang (C++23), CMake ≥ 3.21 installieren.<br>2. [vcpkg](https://github.com/microsoft/vcpkg) klonen, bootstrappen und `VCPKG_ROOT` exportieren.                                                                                         |

#### Kompilieren

```bash
# Windows
cmake --preset windows
cmake --build --preset windows-release
```

```bash
# Linux
cmake --preset linux-release
cmake --build --preset linux-release
```

Der erste Durchlauf dauert einige Minuten (vcpkg kompiliert die Abhängigkeiten); danach greift der Cache.
Die Binaries liegen unter `build/windows/Release/HyDra.exe` bzw. `build/linux-release/HyDra`.

**IDE:** VS Code (CMake Tools) und CLion erkennen die Presets automatisch — Preset wählen, bauen (F7),
starten/debuggen. Die Engine findet ihre Assets unabhängig vom Arbeitsverzeichnis selbst.

---

### Deployment (portables Paket)

```bash
# Windows → build/windows/HyDra-<version>-win64.zip
cmake --build --preset windows-release --target package
```

```bash
# Linux → build/linux-release/HyDra-<version>-Linux.tar.gz
cmake --build --preset linux-release --target package
```

Das Paket enthält Binary, Laufzeit-DLLs (Windows) und den kompletten `assets`-Ordner — entpacken und
`bin/HyDra(.exe)` starten. Windows-Zielsysteme benötigen die _Microsoft Visual C++ Redistributable_.

---

### Bedienung

Die Engine zeigt ein **Echtzeit-Dashboard** mit allen Metriken und der vollständigen Tastenbelegung.

(Screenshots landen in `outputs/screenshots/`, Benchmark-Telemetrie (CSV) in `outputs/benchmarks/`.)
