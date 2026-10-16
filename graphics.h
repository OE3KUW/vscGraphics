#ifndef VSC_GRAPHICS_H
#define VSC_GRAPHICS_H

/* Eine kleine BGI-aehnliche Grafikbibliothek fuer Linux/X11 und Windows. */

#define DETECT 0
#define VGA 9

#define VGALO 0
#define VGAMED 1
#define VGAHI 2

#define grOk 0
#define grNoInitGraph (-1)
#define grNotDetected (-2)
#define grInvalidMode (-3)

/* Die klassischen 16 BGI-Farben. */
enum GraphicsColor
{
    BLACK,
    BLUE,
    GREEN,
    CYAN,
    RED,
    MAGENTA,
    BROWN,
    LIGHTGRAY,
    DARKGRAY,
    LIGHTBLUE,
    LIGHTGREEN,
    LIGHTCYAN,
    LIGHTRED,
    LIGHTMAGENTA,
    YELLOW,
    WHITE
};

enum GraphicsLineStyle
{
    SOLID_LINE,
    DOTTED_LINE,
    CENTER_LINE,
    DASHED_LINE,
    USERBIT_LINE
};

enum GraphicsFillStyle
{
    EMPTY_FILL,
    SOLID_FILL,
    LINE_FILL,
    LTSLASH_FILL,
    SLASH_FILL,
    BKSLASH_FILL,
    LTBKSLASH_FILL,
    HATCH_FILL,
    XHATCH_FILL,
    INTERLEAVE_FILL,
    WIDE_DOT_FILL,
    CLOSE_DOT_FILL,
    USER_FILL
};

enum GraphicsTextDirection
{
    HORIZ_DIR,
    VERT_DIR
};

enum GraphicsTextFont
{
    DEFAULT_FONT,
    TRIPLEX_FONT,
    SMALL_FONT,
    SANS_SERIF_FONT,
    GOTHIC_FONT
};

/* Klassischer Start: Der VGA-Modus bestimmt die Anfangsgroesse. Unter X11
   wird ueber EWMH, unter Windows ueber Win32 maximiert. */
void initgraph(int *graphdriver, int *graphmode, const char *pathtodriver);

/* Praktische Erweiterung fuer eine frei waehlbare Fenstergroesse. */
void initwindow(int width, int height, const char *title);
void closegraph(void);
int graphresult(void);
const char *grapherrormsg(int errorcode);

int getmaxx(void);
int getmaxy(void);
int getx(void);
int gety(void);

void setcolor(int color);
int getcolor(void);
void setbkcolor(int color);
int getbkcolor(void);
void cleardevice(void);

void putpixel(int x, int y, int color);
int getpixel(int x, int y);
void moveto(int x, int y);
void moverel(int dx, int dy);
void line(int x1, int y1, int x2, int y2);
void lineto(int x, int y);
void linerel(int dx, int dy);
void rectangle(int left, int top, int right, int bottom);
void bar(int left, int top, int right, int bottom);
void bar3d(int left, int top, int right, int bottom, int depth, int topflag);
void circle(int x, int y, int radius);
void arc(int x, int y, int startangle, int endangle, int radius);
void ellipse(int x, int y, int startangle, int endangle,
             int xradius, int yradius);
void fillellipse(int x, int y, int xradius, int yradius);
void pieslice(int x, int y, int startangle, int endangle, int radius);

void setlinestyle(int linestyle, unsigned int upattern, int thickness);
void setfillstyle(int pattern, int color);
void settextstyle(int font, int direction, int charsize);
void outtextxy(int x, int y, const char *text);

/* Alles bisher Gezeichnete auf einmal sichtbar machen. */
void flush(void);
void flushgraph(void);

/* Einfache Tastaturfunktionen fuer Grafikprogramme. */
int graphkeypressed(void);
int graphgetch(void);

/* Millisekunden warten, ohne die Fensterdarstellung einzufrieren. */
void graphdelay(unsigned int milliseconds);

#endif
