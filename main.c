#include <stdio.h>

#include "graphics.h"

int main(void)
{
    int graph_driver = DETECT;
    int graph_mode = VGAHI;
    int error;
    int x;
    int y;
    int index;

    /* Wie frueher unter Turbo C: Grafiktreiber automatisch erkennen. */
    initgraph(&graph_driver, &graph_mode, "");
    error = graphresult();
    if (error != grOk)
    {
        fprintf(stderr, "Grafikfehler: %s\n", grapherrormsg(error));
        return 1;
    }

    x = getmaxx();
    y = getmaxy();

    setbkcolor(BLACK);
    cleardevice();

    setcolor(LIGHTBLUE);
    setlinestyle(SOLID_LINE, 0, 3);
    rectangle(8, 8, x - 8, y - 8);

    setcolor(YELLOW);
    settextstyle(DEFAULT_FONT, HORIZ_DIR, 2);
    outtextxy(175, 38, "vscGraphics - BGI-Stil");

    setcolor(DARKGRAY);
    setlinestyle(DOTTED_LINE, 0, 1);
    line(25, 65, x - 25, 65);

    /* Ein Linienfaecher mit wechselnden klassischen BGI-Farben. */
    setlinestyle(SOLID_LINE, 0, 1);
    for (index = 0; index < 16; ++index)
    {
        setcolor(index);
        line(35, 105, 260, 82 + index * 9);
    }

    setcolor(WHITE);
    outtextxy(35, 90, "line()");

    setcolor(LIGHTCYAN);
    setlinestyle(SOLID_LINE, 0, 2);
    circle(350, 145, 58);
    circle(350, 145, 38);
    circle(350, 145, 18);
    outtextxy(320, 225, "circle()");

    setcolor(LIGHTGREEN);
    ellipse(500, 145, 0, 360, 75, 42);
    arc(500, 145, 20, 160, 55);
    outtextxy(460, 225, "ellipse(), arc()");

    setfillstyle(SOLID_FILL, BLUE);
    setcolor(LIGHTBLUE);
    bar3d(45, 285, 165, 370, 18, 1);
    outtextxy(55, 400, "bar3d()");

    setfillstyle(SOLID_FILL, LIGHTMAGENTA);
    setcolor(WHITE);
    fillellipse(290, 330, 70, 42);
    ellipse(290, 330, 0, 360, 70, 42);
    outtextxy(245, 400, "fillellipse()");

    setfillstyle(SOLID_FILL, LIGHTRED);
    setcolor(YELLOW);
    pieslice(455, 330, 25, 305, 62);
    outtextxy(420, 400, "pieslice()");

    setcolor(LIGHTGRAY);
    settextstyle(DEFAULT_FONT, HORIZ_DIR, 1);
    outtextxy(175, y - 35, "Taste druecken oder Fenster schliessen ...");

    /* Bis hier war alles nur im Hintergrundpuffer. Jetzt erscheint es. */
    flush();

    graphgetch();
    closegraph();
    return 0;
}
