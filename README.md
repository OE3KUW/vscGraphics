# vscGraphics

Eine kleine BGI-aehnliche Grafikbibliothek fuer C unter Linux und Windows. Sie
orientiert sich an `graphics.h` aus der Turbo-C-Zeit und verwendet intern:

- unter Linux X11,
- unter Windows die Win32-Grafik GDI.

Die Auswahl erfolgt in `graphics.c` automatisch mit den Praeprozessor-Makros
`__linux__` beziehungsweise `_WIN32`. Die bestehende BGI-kompatible
Schnittstelle in `graphics.h` ist auf beiden Systemen gleich.

## Unter Linux mit VSCodium starten

1. In VS Code **Datei -> Arbeitsbereich aus Datei oeffnen** waehlen.
2. `vscGraphics.code-workspace` oeffnen.
3. `main.c` oeffnen.
4. **F5** druecken. Das Projekt wird zuerst gebaut und danach gestartet.

Alternativ:

- **Strg+Umschalt+B** baut `main.c` und `graphics.c` gemeinsam.
- **Terminal -> Aufgabe ausfuehren -> vscGraphics ausfuehren** baut und startet.

Auf der Kommandozeile:

```bash
gcc -std=gnu17 -Wall -Wextra -g main.c graphics.c -lX11 -lm \
    -o build/vscGraphics.exe
./build/vscGraphics.exe
```

Alternativ mit CMake:

```bash
cmake -S . -B cmake-build
cmake --build cmake-build
./cmake-build/vscGraphics.exe
```

Die Endung `.exe` wird auf dem TERRA25-Stick auch unter Linux verwendet, weil
dessen FAT-Dateisystem Linux-Dateien mit dieser Endung als ausfuehrbar einbindet.
Die Datei ist trotzdem ein Linux-Programm und keine Windows-EXE.

## Grundprinzip

```c
int gd = DETECT;
int gm = VGAHI;

initgraph(&gd, &gm, "");
setbkcolor(BLACK);
cleardevice();

setcolor(YELLOW);
line(10, 10, 200, 100);
circle(300, 200, 50);

flush();       /* Erst jetzt wird das fertige Bild sichtbar. */
graphgetch();  /* Auf eine Taste oder das Schliessen warten. */
closegraph();
```

Alle Zeichenbefehle arbeiten zuerst in einem unsichtbaren Hintergrundpuffer.
`flush()` beziehungsweise `flushgraph()` kopiert das fertige Bild auf einmal
ins Fenster. Dadurch flackert der Aufbau nicht.

`initgraph()` maximiert das Grafikfenster mit Titelleiste und Rahmen. Unter
Linux bittet es den X11-Fenstermanager per EWMH um horizontale und vertikale
Maximierung. Unter Windows wird das Fenster mit `SW_MAXIMIZE` in den
verfuegbaren Arbeitsbereich maximiert. `getmaxx()` und `getmaxy()` beziehen
sich immer auf die aktuelle Zeichenflaeche; auch der Hintergrundpuffer wird
bei einer Groessenaenderung automatisch angepasst. `initwindow()` behaelt
dagegen die explizit angegebene Anfangsgroesse bei.

## Vorhandene Funktionen

- Start/Ende: `initgraph`, `initwindow`, `closegraph`, `graphresult`
- Farben: `setcolor`, `setbkcolor`, die 16 klassischen BGI-Farben
- Pixel/Linien: `putpixel`, `getpixel`, `line`, `moveto`, `lineto`, `linerel`
- Formen: `rectangle`, `bar`, `bar3d`, `circle`, `arc`, `ellipse`,
  `fillellipse`, `pieslice`
- Darstellung: `setlinestyle`, `setfillstyle`, `settextstyle`, `outtextxy`
- Bildausgabe: `flush`, `flushgraph`
- Eingabe/Zeit: `graphkeypressed`, `graphgetch`, `graphdelay`

Hinweis: Die klassischen Fuellmuster werden bereits als Konstanten akzeptiert.
In dieser ersten Version werden alle nichtleeren Muster noch als volle Flaeche
gezeichnet. Vertikale Schrift und benutzerdefinierte Linienmuster folgen in
einer spaeteren Ausbaustufe.

## Voraussetzung unter Linux

Erforderlich sind GCC, X11 und die X11-Entwicklungsdateien. Auf Ubuntu/Mint
heisst das Paket bei Bedarf `libx11-dev`. Auf diesem Rechner ist es bereits
installiert; hier ist keine Nachinstallation notwendig.

## Unter Windows mit CLion bauen

Fuer die Windows-Version ist SFML nicht erforderlich. Win32/GDI gehoert zu
Windows; dadurch muessen keine zusaetzlichen DLL-Dateien neben das Programm
kopiert werden.

1. Auf der GitHub-Seite **Code -> Download ZIP** waehlen und das ZIP entpacken.
2. In CLion **Open** waehlen und den entpackten Ordner mit `CMakeLists.txt`
   oeffnen.
3. Unter **Settings -> Build, Execution, Deployment -> Toolchains** eine
   funktionierende Windows-Toolchain auswaehlen. Die von CLion angebotene
   MinGW-Toolchain ist geeignet.
4. Warten, bis CLion das CMake-Projekt geladen hat.
5. Oben als Ziel **vscGraphics** auswaehlen und auf **Run** klicken.

CMake erkennt Windows automatisch, uebersetzt den `_WIN32`-Zweig und bindet
`user32` sowie `gdi32` ein. Unter Linux erkennt dieselbe Datei X11 und bindet
`X11` sowie `m` ein.

Auf der Windows-Kommandozeile kann alternativ gebaut werden mit:

```powershell
cmake -S . -B cmake-build
cmake --build cmake-build
```

Das fertige Programm liegt je nach CLion-/CMake-Konfiguration beispielsweise
unter `cmake-build-debug/vscGraphics.exe` oder `cmake-build/vscGraphics.exe`.
