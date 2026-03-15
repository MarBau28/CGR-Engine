# HyDra: Hybride Deferred-Rendering-Architektur

**Entwurf und Evaluierung einer hybriden Deferred-Rendering-Architektur zur selektiven, objektbasierten Stilisierung
komplexer 3D-Szenen**

Dieses Repository enthält den C++/OpenGL-Prototypen für die Bachelorarbeit von Marvin Baumann im Studiengang
Medieninformatik (Betreuer: Prof. Dr.-Ing. Gregor Lux).

---

## Motivation und Problemstellung

Die stilisierte Darstellung (Non-Photorealistic Rendering, kurz NPR) von 3D-Szenen und einzelnen Objekten ist in der
Computergrafik sowohl für Anwendungen in der Technik als auch in der Unterhaltung von starkem Interesse. Stilisierung
erhöht die Lesbarkeit und ermöglicht künstlerische Abstraktion.

* Deferred-Shading-Pipelines sind ein oft genutzter Standard für die Darstellung von vielen Objekten, fokussieren sich
  jedoch meist auf Fotorealismus (Physically Based Rendering PBR).
* Da semantische Informationen nach der Rasterisierung verloren gehen, wird NPR in modernen Engines oft zu einem
  globalen Post-Processing-Effekt reduziert.
* Performante Möglichkeiten, Stile selektiv pro Objekt zu steuern (z. B. Bauteil A als Skizze, Bauteil B als Solid),
  ohne auf linear skalierende Forward-Rendering-Methoden zurückzufallen, sind weniger weit verbreitet und oft schwer zu
  implementieren.

## Forschungsfragen

Die Arbeit beschäftigt sich im Kern mit folgenden Leitfragen:

* Wie lässt sich eine Deferred-Rendering-Pipeline so erweitern, dass mehrere verschiedene NPR-Stile/Shader performant
  auf unterschiedliche Objekte in einer Szene angewendet werden können?
* Wie verhält sich dabei die Performance dieser Architektur im Vergleich zur Szenenkomplexität?

## Lösungsansatz

Ziel ist der Entwurf eines semantischen G-Buffers. Die grundlegende Idee besteht darin, den G-Buffer um Stil-Metadaten (
bzw. NPR-Shader-relevante Daten) zu erweitern.
Dies entkoppelt die Shading-Komplexität von der Geometrie und ermöglicht eine hybride Koexistenz verschiedener Stile im
selben Bild. Die Renderzeit soll dabei auch bei hoher Szenenkomplexität stabil bleiben. Gegenüber reinen
Post-Processing-Ansätzen (dem aktuellen Status Quo in Game-Engines) grenzt sich diese Arbeit durch die tiefe
Pipeline-Integration ab.

## Aktueller Implementierungsstand

Das Projekt wird als C++ Anwendung unter der Nutzung der **raylib**-Bibliothek umgesetzt. Die Deferred-Pipeline umfasst
derzeit:

* **Geometry Pass:** Füllt den G-Buffer mit Albedo-, Normalen- und Positionsdaten. Die `Style-ID` wird dabei
  speichereffizient im Alpha-Kanal der Positions-Textur abgelegt.
* **Lighting Pass:** Extrahiert die G-Buffer-Daten und berechnet die Beleuchtung (mit Unterstützung für multiple
  Lichtquellen und Attenuation). Hier findet das Masking und die erste stilistische Weichenstellung basierend auf der
  extrahierten `Style-ID` statt.
* **Post-Processing:** Eine Basis für Kernels und bildschirmfüllende Effekte ist vorbereitet.
* **Debug-View:** Ein integrierter Splitscreen-Modus zur Visualisierung der G-Buffer-Ebenen (Albedo, Normals, World
  Position, Style-ID).

### Definition of Done

Die Evaluierung erfolgt durch die Messung der Frame-Time in Abhängigkeit von der Objektanzahl bei simulierter
instanziierter Geometrie. Ziel ist der Nachweis, dass die Architektur prinzipiell besser skaliert als naive
Forward-Ansätze.