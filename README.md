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

- **Sprache:** C++20
- **Graphics API:** OpenGL 3.3+ (Core Profile)
- **Libraries:** raylib (Windowing & Math), glad (Extension Loading)
