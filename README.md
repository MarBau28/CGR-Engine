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
- **Libraries:** raylib (Windowing & Math), glad (Extension Loading)

---

### Ausführung der Release-Builds

Die bereitgestellten Release-Pakete (`.zip` für Windows, `.tar.gz` für Linux) sind vollständig autark ("Self-Contained")
und erfordern keine Installation.

1. **Voraussetzungen:** Eine dedizierte oder integrierte Grafikkarte mit Unterstützung für OpenGL 3.3.
2. **Ausführung:**
    - Archiv entpacken.
    - In den generierten Ordner navigieren und den Ordner `bin/` öffnen.
    - Die ausführbare Datei `HyDra` (bzw. `HyDra.exe` auf Windows) per Doppelklick starten.

*(Es müssen keine Umgebungsvariablen gesetzt oder Abhängigkeiten installiert werden).*

---

### Kompilierung aus dem Quellcode (Entwickler-Leitfaden)

Die Engine nutzt **CMake** für das Build-System und **vcpkg** (im Manifest Mode) für die Verwaltung der C++
Abhängigkeiten (Raylib, GLAD, GLFW).

#### Voraussetzungen (Systemübergreifend)

- Ein C++23 kompatibler Compiler (GCC/Clang für Linux, MSVC für Windows).
- **CMake** (Mindestens Version 3.15).
- **Git** (Erforderlich für vcpkg, um Abhängigkeiten zu klonen).

#### Spezifische Windows / VS Code Fehlerbehebung

Wenn Sie beim Öffnen des Projekts in VS Code unter Windows den Fehler **"CMake could not be found"** erhalten, liegt
dies an einer nicht vollständig konfigurierten C++ Entwicklungsumgebung.

Lösung:

1. Installieren Sie die [Visual Studio Build Tools](https://visualstudio.microsoft.com/de/visual-cpp-build-tools/).
   Wählen Sie im Installer zwingend den Workload **"Desktopentwicklung mit C++"** aus. Dies installiert den MSVC
   Compiler und CMake.
2. *(Wichtig für vcpkg)*: Stellen Sie sicher, dass im VS Installer unter "Sprachpakete" das **Englische Sprachpaket**
   installiert ist.
3. Installieren Sie in VS Code die Erweiterungen **"C/C++"** (Microsoft) und **"CMake Tools"** (Microsoft).
4. Nach einem Neustart von VS Code sollte das Projekt automatisch konfiguriert werden. Sie können in der unteren blauen
   Leiste das Toolkit (z.B. `GCC` oder `Visual Studio amd64`) auswählen und auf "Build" klicken.

#### Manuelles Bauen über die Kommandozeile

```bash
# Release-Build (Erzeugt die Binaries im cmake-build-release Ordner)
cmake --build cmake-build-release --config Release -j 8

# Paketieren (Erzeugt das .zip oder .tar.gz Release-Archiv)
cmake --build cmake-build-release --target package
```
