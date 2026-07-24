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

### Bedienung & Dashboard

Die Engine verfügt über ein integriertes **Echtzeit-Dashboard**, um die Auswirkungen der Architektur-Wechsel unmittelbar
zu visualisieren.


---

#### Tech Stack

- **Sprache:** C++23
- **Graphics API:** OpenGL 3.3+ (Core Profile)
- **Libraries:** GLFW (Windowing & Input), glad (Extension Loading), glm (Math), raylib, stb, asio
- **Dependency Management:** vcpkg (Manifest-Modus), CMake ≥ 3.21 mit Presets

---

### Build-Anleitung

Die Abhängigkeiten werden über den vcpkg-Manifest-Modus (`vcpkg.json`) **automatisch beim
CMake-Configure** installiert — ein manueller `vcpkg install`-Aufruf ist nicht nötig.
Einzige Voraussetzung: Die Umgebungsvariable `VCPKG_ROOT` muss auf eine vcpkg-Installation zeigen.

#### Voraussetzungen

| Plattform | Anforderungen |
|---|---|
| **Windows** | Visual Studio 2022+ (MSVC, C++-Workload). `VCPKG_ROOT` auf das VS-gebündelte vcpkg setzen, z. B. `C:\Program Files\Microsoft Visual Studio\18\Community\VC\vcpkg` |
| **Linux** | GCC/Clang mit C++23, CMake ≥ 3.21, [vcpkg](https://github.com/microsoft/vcpkg) geklont und `VCPKG_ROOT` exportiert |

> **Hinweis (Windows):** Nach dem Setzen von `VCPKG_ROOT` die IDE/das Terminal neu starten,
> damit die Variable sichtbar wird. Befehle in einer *Developer PowerShell for VS* ausführen,
> da `cmake` dort im PATH liegt.

#### Konfigurieren & Bauen (CMake Presets)

```bash
# Windows
cmake --preset windows
cmake --build --preset windows-release

# Linux
cmake --preset linux-release
cmake --build --preset linux-release
```

Die Build-Ausgabe liegt unter `build/<preset-name>/`.

#### Portables Paket erstellen (CPack)

```bash
# Windows → build/windows/HyDra-<version>-win64.zip (inkl. Runtime-DLLs)
cmake --build --preset windows-release --target package

# Linux → build/linux-release/HyDra-<version>-Linux.tar.gz
cmake --build --preset linux-release --target package
```

> Das Windows-Paket setzt die *Microsoft Visual C++ Redistributable* auf dem Zielsystem voraus.
