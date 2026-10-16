# vscGraphics

Eine kleine BGI-aehnliche Grafikbibliothek fuer C unter Linux. Sie orientiert
sich an `graphics.h` aus der Turbo-C-Zeit, verwendet intern aber X11.

## Projekt starten

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

`initgraph()` bittet den X11-Fenstermanager per EWMH um horizontale und
vertikale Maximierung. Das Fenster behaelt dabei Titelleiste und Rahmen und
nutzt den vom Desktop bereitgestellten Arbeitsbereich. `getmaxx()` und
`getmaxy()` beziehen sich immer auf die aktuelle Zeichenflaeche; auch der
Hintergrundpuffer wird bei einer Groessenaenderung automatisch angepasst.
`initwindow()` behaelt dagegen die explizit angegebene Anfangsgroesse bei.

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
