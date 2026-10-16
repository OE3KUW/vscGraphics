#include "graphics.h"

#if defined(__linux__)

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <math.h>
#include <poll.h>
#include <stddef.h>
#include <string.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static Display *display;
static int screen_number;
static Window window;
static Pixmap back_buffer;
static GC graphics_context;
static Atom wm_delete_window;
static XFontStruct *text_font;

static int window_width;
static int window_height;
static int current_x;
static int current_y;
static int foreground_color = WHITE;
static int background_color = BLACK;
static int fill_color = WHITE;
static int fill_pattern = SOLID_FILL;
static int last_error = grNoInitGraph;
static int buffered_key = -1;
static unsigned long palette[16];

static const unsigned short palette_rgb[16][3] = {
    {0x0000, 0x0000, 0x0000}, {0x0000, 0x0000, 0xaaaa},
    {0x0000, 0xaaaa, 0x0000}, {0x0000, 0xaaaa, 0xaaaa},
    {0xaaaa, 0x0000, 0x0000}, {0xaaaa, 0x0000, 0xaaaa},
    {0xaaaa, 0x5555, 0x0000}, {0xaaaa, 0xaaaa, 0xaaaa},
    {0x5555, 0x5555, 0x5555}, {0x5555, 0x5555, 0xffff},
    {0x5555, 0xffff, 0x5555}, {0x5555, 0xffff, 0xffff},
    {0xffff, 0x5555, 0x5555}, {0xffff, 0x5555, 0xffff},
    {0xffff, 0xffff, 0x5555}, {0xffff, 0xffff, 0xffff}
};

static int valid_color(int color)
{
    return color >= BLACK && color <= WHITE;
}

static int is_ready(void)
{
    return display != NULL && window != 0 && back_buffer != 0;
}

static void use_color(int color)
{
    if (is_ready() && valid_color(color))
        XSetForeground(display, graphics_context, palette[color]);
}

static void restore_foreground(void)
{
    use_color(foreground_color);
}

static void resize_back_buffer(int width, int height)
{
    Pixmap new_buffer;
    int copy_width;
    int copy_height;

    if (width <= 0 || height <= 0 ||
        (width == window_width && height == window_height))
        return;

    /* During the initial map/maximize sequence the GC and pixmap do not yet
       exist. Remembering the client size is sufficient in that phase. */
    if (graphics_context == 0 || back_buffer == 0)
    {
        window_width = width;
        window_height = height;
        return;
    }

    new_buffer = XCreatePixmap(display, window, (unsigned int)width,
                               (unsigned int)height,
                               (unsigned int)DefaultDepth(display,
                                                          screen_number));
    if (new_buffer == 0)
        return;

    XSetForeground(display, graphics_context, palette[background_color]);
    XFillRectangle(display, new_buffer, graphics_context, 0, 0,
                   (unsigned int)width, (unsigned int)height);

    copy_width = width < window_width ? width : window_width;
    copy_height = height < window_height ? height : window_height;
    XCopyArea(display, back_buffer, new_buffer, graphics_context,
              0, 0, (unsigned int)copy_width, (unsigned int)copy_height,
              0, 0);

    XFreePixmap(display, back_buffer);
    back_buffer = new_buffer;
    window_width = width;
    window_height = height;
    restore_foreground();
}

static void repaint_window(void)
{
    if (!is_ready())
        return;

    XCopyArea(display, back_buffer, window, graphics_context,
              0, 0, (unsigned int)window_width, (unsigned int)window_height,
              0, 0);
    XFlush(display);
}

static void remember_key(XKeyEvent *key_event)
{
    char text[8];
    KeySym symbol;
    int count;

    if (buffered_key >= 0)
        return;

    count = XLookupString(key_event, text, (int)sizeof(text), &symbol, NULL);
    if (count > 0)
        buffered_key = (unsigned char)text[0];
    else if (symbol == XK_Escape)
        buffered_key = 27;
    else if (symbol >= XK_Left && symbol <= XK_Down)
        buffered_key = (int)symbol;
}

static void process_event(XEvent *event)
{
    if (event->type == Expose && event->xexpose.count == 0)
        repaint_window();
    else if (event->type == ConfigureNotify &&
             event->xconfigure.window == window)
    {
        resize_back_buffer(event->xconfigure.width, event->xconfigure.height);
        repaint_window();
    }
    else if (event->type == KeyPress)
        remember_key(&event->xkey);
    else if (event->type == ClientMessage &&
             (Atom)event->xclient.data.l[0] == wm_delete_window)
        buffered_key = 27;
}

static void pump_events(void)
{
    XEvent event;

    if (display == NULL)
        return;

    while (XPending(display) > 0)
    {
        XNextEvent(display, &event);
        process_event(&event);
    }
}

static void allocate_palette(void)
{
    Colormap colormap = DefaultColormap(display, screen_number);
    int index;

    for (index = 0; index < 16; ++index)
    {
        XColor color;
        color.red = palette_rgb[index][0];
        color.green = palette_rgb[index][1];
        color.blue = palette_rgb[index][2];
        color.flags = DoRed | DoGreen | DoBlue;

        if (!XAllocColor(display, colormap, &color))
            color.pixel = (index == BLACK)
                              ? BlackPixel(display, screen_number)
                              : WhitePixel(display, screen_number);
        palette[index] = color.pixel;
    }
}

static long long monotonic_milliseconds(void)
{
    struct timespec now;

    clock_gettime(CLOCK_MONOTONIC, &now);
    return (long long)now.tv_sec * 1000LL + now.tv_nsec / 1000000LL;
}

static int atom_list_contains(Window target, Atom property, Atom wanted)
{
    Atom actual_type;
    int actual_format;
    unsigned long item_count;
    unsigned long bytes_after;
    unsigned char *data = NULL;
    unsigned long index;
    int found = 0;

    if (XGetWindowProperty(display, target, property, 0, 1024, False,
                           XA_ATOM, &actual_type, &actual_format, &item_count,
                           &bytes_after, &data) != Success)
        return 0;

    if (actual_type == XA_ATOM && actual_format == 32)
    {
        Atom *atoms = (Atom *)data;
        for (index = 0; index < item_count; ++index)
            if (atoms[index] == wanted)
            {
                found = 1;
                break;
            }
    }
    if (data != NULL)
        XFree(data);
    return found;
}

static int window_is_maximized(Atom wm_state, Atom maximized_horz,
                               Atom maximized_vert)
{
    return atom_list_contains(window, wm_state, maximized_horz) &&
           atom_list_contains(window, wm_state, maximized_vert);
}

static void request_maximized_window(void)
{
    Window root = RootWindow(display, screen_number);
    Atom net_supported = XInternAtom(display, "_NET_SUPPORTED", False);
    Atom wm_state = XInternAtom(display, "_NET_WM_STATE", False);
    Atom maximized_horz =
        XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
    Atom maximized_vert =
        XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_VERT", False);
    XEvent request;
    long long deadline;
    long long last_resize;
    int maximized = 0;

    /* Do not wait for a window manager which does not advertise EWMH
       maximization (for example a bare X server used by automated tests). */
    if (!atom_list_contains(root, net_supported, wm_state) ||
        !atom_list_contains(root, net_supported, maximized_horz) ||
        !atom_list_contains(root, net_supported, maximized_vert))
        return;

    memset(&request, 0, sizeof(request));
    request.xclient.type = ClientMessage;
    request.xclient.window = window;
    request.xclient.message_type = wm_state;
    request.xclient.format = 32;
    request.xclient.data.l[0] = 1; /* _NET_WM_STATE_ADD */
    request.xclient.data.l[1] = (long)maximized_horz;
    request.xclient.data.l[2] = (long)maximized_vert;
    request.xclient.data.l[3] = 1; /* normal application */

    XSendEvent(display, root, False,
               SubstructureRedirectMask | SubstructureNotifyMask, &request);
    XFlush(display);

    deadline = monotonic_milliseconds() + 2000;
    last_resize = monotonic_milliseconds();
    while (monotonic_milliseconds() < deadline)
    {
        while (XPending(display) > 0)
        {
            XEvent event;
            XNextEvent(display, &event);
            if (event.type == ConfigureNotify &&
                event.xconfigure.window == window)
                last_resize = monotonic_milliseconds();
            process_event(&event);
        }

        maximized = window_is_maximized(wm_state, maximized_horz,
                                        maximized_vert);
        if (maximized && monotonic_milliseconds() - last_resize >= 100)
            break;

        {
            struct pollfd connection;
            connection.fd = ConnectionNumber(display);
            connection.events = POLLIN;
            connection.revents = 0;
            poll(&connection, 1, 20);
        }
    }
}

static void create_window(int width, int height, const char *title,
                          int maximize)
{
    XEvent event;
    XSizeHints size_hints;
    XWindowAttributes attributes;

    if (is_ready())
        closegraph();

    last_error = grNoInitGraph;
    buffered_key = -1;
    if (width < 100 || height < 100)
    {
        last_error = grInvalidMode;
        return;
    }

    display = XOpenDisplay(NULL);
    if (display == NULL)
    {
        last_error = grNotDetected;
        return;
    }

    screen_number = DefaultScreen(display);
    window_width = width;
    window_height = height;
    window = XCreateSimpleWindow(
        display, RootWindow(display, screen_number),
        80, 80, (unsigned int)width, (unsigned int)height, 1,
        WhitePixel(display, screen_number), BlackPixel(display, screen_number));

    if (window == 0)
    {
        XCloseDisplay(display);
        display = NULL;
        return;
    }

    XStoreName(display, window, title != NULL ? title : "vscGraphics");
    XSelectInput(display, window, ExposureMask | KeyPressMask |
                                      StructureNotifyMask);

    size_hints.flags = PMinSize;
    size_hints.min_width = 100;
    size_hints.min_height = 100;
    XSetWMNormalHints(display, window, &size_hints);

    wm_delete_window = XInternAtom(display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(display, window, &wm_delete_window, 1);
    XMapWindow(display, window);

    do
    {
        XNextEvent(display, &event);
    } while (event.type != MapNotify);

    if (maximize)
        request_maximized_window();

    /* Width and height are client-area dimensions. The window manager keeps
       title bar, frame and desktop panels outside this usable drawing area. */
    if (XGetWindowAttributes(display, window, &attributes) != 0)
    {
        window_width = attributes.width;
        window_height = attributes.height;
    }

    graphics_context = XCreateGC(display, window, 0, NULL);
    back_buffer = XCreatePixmap(display, window,
                                (unsigned int)window_width,
                                (unsigned int)window_height,
                                (unsigned int)DefaultDepth(display, screen_number));
    if (graphics_context == 0 || back_buffer == 0)
    {
        closegraph();
        return;
    }

    allocate_palette();
    foreground_color = WHITE;
    background_color = BLACK;
    fill_color = WHITE;
    fill_pattern = SOLID_FILL;
    current_x = 0;
    current_y = 0;
    last_error = grOk;

    use_color(background_color);
    XFillRectangle(display, back_buffer, graphics_context, 0, 0,
                   (unsigned int)window_width, (unsigned int)window_height);
    restore_foreground();
    settextstyle(DEFAULT_FONT, HORIZ_DIR, 1);
    repaint_window();
}

void initwindow(int width, int height, const char *title)
{
    create_window(width, height, title, 0);
}

void initgraph(int *graphdriver, int *graphmode, const char *pathtodriver)
{
    int width = 640;
    int height = 480;
    int mode = VGAHI;

    (void)pathtodriver;
    if (graphmode != NULL)
        mode = *graphmode;

    if (mode == VGALO)
    {
        width = 640;
        height = 200;
    }
    else if (mode == VGAMED)
    {
        width = 640;
        height = 350;
    }
    else if (mode != VGAHI)
    {
        last_error = grInvalidMode;
        return;
    }

    if (graphdriver != NULL)
        *graphdriver = VGA;
    if (graphmode != NULL)
        *graphmode = mode;
    create_window(width, height, "vscGraphics - BGI Demo", 1);
}

void closegraph(void)
{
    if (display == NULL)
        return;

    if (text_font != NULL)
    {
        XFreeFont(display, text_font);
        text_font = NULL;
    }
    if (back_buffer != 0)
        XFreePixmap(display, back_buffer);
    if (graphics_context != 0)
        XFreeGC(display, graphics_context);
    if (window != 0)
        XDestroyWindow(display, window);
    XCloseDisplay(display);

    display = NULL;
    window = 0;
    back_buffer = 0;
    graphics_context = 0;
    buffered_key = -1;
    last_error = grNoInitGraph;
}

int graphresult(void)
{
    return last_error;
}

const char *grapherrormsg(int errorcode)
{
    switch (errorcode)
    {
    case grOk:
        return "Kein Grafikfehler";
    case grNoInitGraph:
        return "Grafik wurde nicht initialisiert";
    case grNotDetected:
        return "Keine X11-Anzeige gefunden";
    case grInvalidMode:
        return "Ungueltiger Grafikmodus";
    default:
        return "Unbekannter Grafikfehler";
    }
}

int getmaxx(void) { return window_width - 1; }
int getmaxy(void) { return window_height - 1; }
int getx(void) { return current_x; }
int gety(void) { return current_y; }

void setcolor(int color)
{
    if (!valid_color(color))
        return;
    foreground_color = color;
    restore_foreground();
}

int getcolor(void) { return foreground_color; }

void setbkcolor(int color)
{
    if (valid_color(color))
        background_color = color;
}

int getbkcolor(void) { return background_color; }

void cleardevice(void)
{
    if (!is_ready())
        return;
    use_color(background_color);
    XFillRectangle(display, back_buffer, graphics_context, 0, 0,
                   (unsigned int)window_width, (unsigned int)window_height);
    restore_foreground();
    current_x = 0;
    current_y = 0;
}

void putpixel(int x, int y, int color)
{
    if (!is_ready() || !valid_color(color))
        return;
    use_color(color);
    XDrawPoint(display, back_buffer, graphics_context, x, y);
    restore_foreground();
}

int getpixel(int x, int y)
{
    XImage *image;
    unsigned long pixel;
    int index;

    if (!is_ready() || x < 0 || y < 0 || x >= window_width || y >= window_height)
        return -1;
    image = XGetImage(display, back_buffer, x, y, 1, 1, AllPlanes, ZPixmap);
    if (image == NULL)
        return -1;
    pixel = XGetPixel(image, 0, 0);
    XDestroyImage(image);

    for (index = 0; index < 16; ++index)
        if (palette[index] == pixel)
            return index;
    return -1;
}

void moveto(int x, int y)
{
    current_x = x;
    current_y = y;
}

void moverel(int dx, int dy) { moveto(current_x + dx, current_y + dy); }

void line(int x1, int y1, int x2, int y2)
{
    if (!is_ready())
        return;
    XDrawLine(display, back_buffer, graphics_context, x1, y1, x2, y2);
    current_x = x2;
    current_y = y2;
}

void lineto(int x, int y) { line(current_x, current_y, x, y); }
void linerel(int dx, int dy) { lineto(current_x + dx, current_y + dy); }

static void ordered_box(int *left, int *top, int *right, int *bottom)
{
    int temporary;
    if (*left > *right)
    {
        temporary = *left;
        *left = *right;
        *right = temporary;
    }
    if (*top > *bottom)
    {
        temporary = *top;
        *top = *bottom;
        *bottom = temporary;
    }
}

void rectangle(int left, int top, int right, int bottom)
{
    if (!is_ready())
        return;
    ordered_box(&left, &top, &right, &bottom);
    XDrawRectangle(display, back_buffer, graphics_context, left, top,
                   (unsigned int)(right - left), (unsigned int)(bottom - top));
}

void bar(int left, int top, int right, int bottom)
{
    if (!is_ready())
        return;
    ordered_box(&left, &top, &right, &bottom);
    if (fill_pattern == EMPTY_FILL)
    {
        rectangle(left, top, right, bottom);
        return;
    }
    use_color(fill_color);
    XFillRectangle(display, back_buffer, graphics_context, left, top,
                   (unsigned int)(right - left + 1),
                   (unsigned int)(bottom - top + 1));
    restore_foreground();
}

void bar3d(int left, int top, int right, int bottom, int depth, int topflag)
{
    bar(left, top, right, bottom);
    rectangle(left, top, right, bottom);
    line(right, top, right + depth, top - depth);
    line(right, bottom, right + depth, bottom - depth);
    line(right + depth, top - depth, right + depth, bottom - depth);
    if (topflag)
    {
        line(left, top, left + depth, top - depth);
        line(left + depth, top - depth, right + depth, top - depth);
    }
}

void circle(int x, int y, int radius)
{
    ellipse(x, y, 0, 360, radius, radius);
}

void ellipse(int x, int y, int startangle, int endangle,
             int xradius, int yradius)
{
    if (!is_ready() || xradius < 0 || yradius < 0)
        return;
    XDrawArc(display, back_buffer, graphics_context,
             x - xradius, y - yradius,
             (unsigned int)(2 * xradius), (unsigned int)(2 * yradius),
             startangle * 64, (endangle - startangle) * 64);
}

void arc(int x, int y, int startangle, int endangle, int radius)
{
    ellipse(x, y, startangle, endangle, radius, radius);
}

void fillellipse(int x, int y, int xradius, int yradius)
{
    if (!is_ready() || xradius < 0 || yradius < 0)
        return;
    use_color(fill_color);
    XFillArc(display, back_buffer, graphics_context,
             x - xradius, y - yradius,
             (unsigned int)(2 * xradius), (unsigned int)(2 * yradius),
             0, 360 * 64);
    restore_foreground();
}

void pieslice(int x, int y, int startangle, int endangle, int radius)
{
    if (!is_ready() || radius < 0)
        return;
    XSetArcMode(display, graphics_context, ArcPieSlice);
    use_color(fill_color);
    XFillArc(display, back_buffer, graphics_context,
             x - radius, y - radius,
             (unsigned int)(2 * radius), (unsigned int)(2 * radius),
             startangle * 64, (endangle - startangle) * 64);
    restore_foreground();
    XDrawArc(display, back_buffer, graphics_context,
             x - radius, y - radius,
             (unsigned int)(2 * radius), (unsigned int)(2 * radius),
             startangle * 64, (endangle - startangle) * 64);
}

void setlinestyle(int linestyle, unsigned int upattern, int thickness)
{
    static const char dotted[] = {1, 3};
    static const char centered[] = {6, 3, 1, 3};
    static const char dashed[] = {7, 4};
    int width = thickness > 0 ? thickness : 1;

    (void)upattern;
    if (!is_ready())
        return;

    if (linestyle == SOLID_LINE || linestyle == USERBIT_LINE)
        XSetLineAttributes(display, graphics_context, (unsigned int)width,
                           LineSolid, CapRound, JoinRound);
    else
    {
        const char *dashes = dotted;
        int count = (int)sizeof(dotted);
        if (linestyle == CENTER_LINE)
        {
            dashes = centered;
            count = (int)sizeof(centered);
        }
        else if (linestyle == DASHED_LINE)
        {
            dashes = dashed;
            count = (int)sizeof(dashed);
        }
        XSetLineAttributes(display, graphics_context, (unsigned int)width,
                           LineOnOffDash, CapRound, JoinRound);
        XSetDashes(display, graphics_context, 0, dashes, count);
    }
}

void setfillstyle(int pattern, int color)
{
    if (pattern >= EMPTY_FILL && pattern <= USER_FILL)
        fill_pattern = pattern;
    if (valid_color(color))
        fill_color = color;
}

void settextstyle(int font, int direction, int charsize)
{
    const char *font_name;

    (void)font;
    (void)direction;
    if (!is_ready())
        return;

    font_name = charsize >= 2 ? "10x20" : "9x15";
    if (text_font != NULL)
        XFreeFont(display, text_font);
    text_font = XLoadQueryFont(display, font_name);
    if (text_font == NULL)
        text_font = XLoadQueryFont(display, "fixed");
    if (text_font != NULL)
        XSetFont(display, graphics_context, text_font->fid);
}

void outtextxy(int x, int y, const char *text)
{
    if (!is_ready() || text == NULL)
        return;
    XDrawString(display, back_buffer, graphics_context, x, y,
                text, (int)strlen(text));
}

void flushgraph(void)
{
    repaint_window();
    pump_events();
}

void flush(void) { flushgraph(); }

int graphkeypressed(void)
{
    pump_events();
    return buffered_key >= 0;
}

int graphgetch(void)
{
    XEvent event;
    int key;

    if (!is_ready())
        return -1;
    while (buffered_key < 0)
    {
        XNextEvent(display, &event);
        process_event(&event);
    }
    key = buffered_key;
    buffered_key = -1;
    return key;
}

void graphdelay(unsigned int milliseconds)
{
    struct timespec request;
    unsigned int remaining = milliseconds;

    while (remaining > 0)
    {
        unsigned int part = remaining > 10 ? 10 : remaining;
        request.tv_sec = 0;
        request.tv_nsec = (long)part * 1000000L;
        nanosleep(&request, NULL);
        pump_events();
        remaining -= part;
    }
}

#elif defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <math.h>
#include <stddef.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static HWND window;
static HDC back_buffer_dc;
static HBITMAP back_buffer_bitmap;
static HBITMAP original_bitmap;
static HFONT text_font;
static int window_width;
static int window_height;
static int current_x;
static int current_y;
static int foreground_color = WHITE;
static int background_color = BLACK;
static int fill_color = WHITE;
static int fill_pattern = SOLID_FILL;
static int line_style = SOLID_LINE;
static int line_thickness = 1;
static int text_direction = HORIZ_DIR;
static int text_size = 1;
static int last_error = grNoInitGraph;
static int buffered_key = -1;

static const COLORREF palette[16] = {
    RGB(0, 0, 0),       RGB(0, 0, 170),     RGB(0, 170, 0),
    RGB(0, 170, 170),   RGB(170, 0, 0),     RGB(170, 0, 170),
    RGB(170, 85, 0),    RGB(170, 170, 170), RGB(85, 85, 85),
    RGB(85, 85, 255),   RGB(85, 255, 85),   RGB(85, 255, 255),
    RGB(255, 85, 85),   RGB(255, 85, 255),  RGB(255, 255, 85),
    RGB(255, 255, 255)
};

static int valid_color(int color)
{
    return color >= BLACK && color <= WHITE;
}

static int is_ready(void)
{
    return window != NULL && back_buffer_dc != NULL &&
           back_buffer_bitmap != NULL;
}

static void ordered_box(int *left, int *top, int *right, int *bottom)
{
    int temporary;
    if (*left > *right)
    {
        temporary = *left;
        *left = *right;
        *right = temporary;
    }
    if (*top > *bottom)
    {
        temporary = *top;
        *top = *bottom;
        *bottom = temporary;
    }
}

static int native_pen_style(void)
{
    switch (line_style)
    {
    case DOTTED_LINE:
        return PS_DOT;
    case CENTER_LINE:
        return PS_DASHDOT;
    case DASHED_LINE:
        return PS_DASH;
    default:
        return PS_SOLID;
    }
}

static HPEN select_pen(HDC dc, int color, HPEN *old_pen)
{
    HPEN pen = CreatePen(native_pen_style(), line_thickness,
                         palette[valid_color(color) ? color : WHITE]);
    if (pen == NULL)
        pen = (HPEN)GetStockObject(WHITE_PEN);
    *old_pen = (HPEN)SelectObject(dc, pen);
    return pen;
}

static void release_pen(HDC dc, HPEN pen, HPEN old_pen)
{
    SelectObject(dc, old_pen);
    if (pen != GetStockObject(WHITE_PEN))
        DeleteObject(pen);
}

static void clear_dc(HDC dc, int width, int height)
{
    RECT area = {0, 0, width, height};
    HBRUSH brush = CreateSolidBrush(palette[background_color]);
    FillRect(dc, &area, brush);
    DeleteObject(brush);
}

static void resize_back_buffer(int width, int height)
{
    HDC window_dc;
    HDC new_dc;
    HBITMAP new_bitmap;
    HBITMAP new_original;
    int copy_width;
    int copy_height;

    if (window == NULL || width <= 0 || height <= 0)
        return;
    if (back_buffer_dc != NULL && width == window_width &&
        height == window_height)
        return;

    window_dc = GetDC(window);
    if (window_dc == NULL)
        return;
    new_dc = CreateCompatibleDC(window_dc);
    new_bitmap = CreateCompatibleBitmap(window_dc, width, height);
    ReleaseDC(window, window_dc);
    if (new_dc == NULL || new_bitmap == NULL)
    {
        if (new_bitmap != NULL)
            DeleteObject(new_bitmap);
        if (new_dc != NULL)
            DeleteDC(new_dc);
        return;
    }

    new_original = (HBITMAP)SelectObject(new_dc, new_bitmap);
    clear_dc(new_dc, width, height);

    if (back_buffer_dc != NULL && back_buffer_bitmap != NULL)
    {
        copy_width = width < window_width ? width : window_width;
        copy_height = height < window_height ? height : window_height;
        BitBlt(new_dc, 0, 0, copy_width, copy_height,
               back_buffer_dc, 0, 0, SRCCOPY);
        SelectObject(back_buffer_dc, original_bitmap);
        DeleteObject(back_buffer_bitmap);
        DeleteDC(back_buffer_dc);
    }

    back_buffer_dc = new_dc;
    back_buffer_bitmap = new_bitmap;
    original_bitmap = new_original;
    window_width = width;
    window_height = height;
    SetBkMode(back_buffer_dc, TRANSPARENT);
    if (text_font != NULL)
        SelectObject(back_buffer_dc, text_font);
}

static void repaint_window(void)
{
    HDC window_dc;

    if (!is_ready())
        return;
    window_dc = GetDC(window);
    if (window_dc != NULL)
    {
        BitBlt(window_dc, 0, 0, window_width, window_height,
               back_buffer_dc, 0, 0, SRCCOPY);
        ReleaseDC(window, window_dc);
    }
}

static void remember_virtual_key(WPARAM key)
{
    if (buffered_key >= 0)
        return;
    if (key == VK_ESCAPE)
        buffered_key = 27;
    else if (key == VK_LEFT || key == VK_UP ||
             key == VK_RIGHT || key == VK_DOWN)
        buffered_key = (int)key;
}

static LRESULT CALLBACK graphics_window_proc(HWND hwnd, UINT message,
                                              WPARAM wparam, LPARAM lparam)
{
    switch (message)
    {
    case WM_ERASEBKGND:
        return 1;
    case WM_SIZE:
        if (wparam != SIZE_MINIMIZED)
            resize_back_buffer(LOWORD(lparam), HIWORD(lparam));
        return 0;
    case WM_PAINT:
    {
        PAINTSTRUCT paint;
        HDC paint_dc = BeginPaint(hwnd, &paint);
        if (back_buffer_dc != NULL)
            BitBlt(paint_dc, 0, 0, window_width, window_height,
                   back_buffer_dc, 0, 0, SRCCOPY);
        EndPaint(hwnd, &paint);
        return 0;
    }
    case WM_CHAR:
        if (buffered_key < 0 && wparam > 0 && wparam <= 255)
            buffered_key = (int)wparam;
        return 0;
    case WM_KEYDOWN:
        remember_virtual_key(wparam);
        return 0;
    case WM_CLOSE:
        if (buffered_key < 0)
            buffered_key = 27;
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        window = NULL;
        return 0;
    default:
        return DefWindowProcA(hwnd, message, wparam, lparam);
    }
}

static int register_window_class(void)
{
    static int registered;
    WNDCLASSA window_class;
    HINSTANCE instance;

    if (registered)
        return 1;
    instance = GetModuleHandleA(NULL);
    memset(&window_class, 0, sizeof(window_class));
    window_class.style = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = graphics_window_proc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursor(NULL, IDC_ARROW);
    window_class.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    window_class.lpszClassName = "vscGraphicsWindow";
    if (RegisterClassA(&window_class) == 0 &&
        GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        return 0;
    registered = 1;
    return 1;
}

static void pump_events(void)
{
    MSG message;

    while (PeekMessageA(&message, NULL, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&message);
        DispatchMessageA(&message);
    }
}

static void create_window(int width, int height, const char *title,
                          int maximize)
{
    RECT outer;
    RECT client;
    DWORD style = WS_OVERLAPPEDWINDOW;
    HINSTANCE instance = GetModuleHandleA(NULL);

    if (back_buffer_dc != NULL || window != NULL)
        closegraph();
    last_error = grNoInitGraph;
    buffered_key = -1;
    if (width < 100 || height < 100)
    {
        last_error = grInvalidMode;
        return;
    }
    if (!register_window_class())
    {
        last_error = grNotDetected;
        return;
    }

    outer.left = 0;
    outer.top = 0;
    outer.right = width;
    outer.bottom = height;
    AdjustWindowRectEx(&outer, style, FALSE, 0);
    window = CreateWindowExA(
        0, "vscGraphicsWindow",
        title != NULL ? title : "vscGraphics",
        style, CW_USEDEFAULT, CW_USEDEFAULT,
        outer.right - outer.left, outer.bottom - outer.top,
        NULL, NULL, instance, NULL);
    if (window == NULL)
    {
        last_error = grNotDetected;
        return;
    }

    window_width = width;
    window_height = height;
    ShowWindow(window, maximize ? SW_MAXIMIZE : SW_SHOWNORMAL);
    UpdateWindow(window);
    pump_events();
    GetClientRect(window, &client);
    resize_back_buffer(client.right - client.left, client.bottom - client.top);
    if (!is_ready())
    {
        closegraph();
        return;
    }

    foreground_color = WHITE;
    background_color = BLACK;
    fill_color = WHITE;
    fill_pattern = SOLID_FILL;
    line_style = SOLID_LINE;
    line_thickness = 1;
    current_x = 0;
    current_y = 0;
    last_error = grOk;
    clear_dc(back_buffer_dc, window_width, window_height);
    settextstyle(DEFAULT_FONT, HORIZ_DIR, 1);
    repaint_window();
    SetFocus(window);
}

void initwindow(int width, int height, const char *title)
{
    create_window(width, height, title, 0);
}

void initgraph(int *graphdriver, int *graphmode, const char *pathtodriver)
{
    int width = 640;
    int height = 480;
    int mode = VGAHI;

    (void)pathtodriver;
    if (graphmode != NULL)
        mode = *graphmode;
    if (mode == VGALO)
        height = 200;
    else if (mode == VGAMED)
        height = 350;
    else if (mode != VGAHI)
    {
        last_error = grInvalidMode;
        return;
    }
    if (graphdriver != NULL)
        *graphdriver = VGA;
    if (graphmode != NULL)
        *graphmode = mode;
    create_window(width, height, "vscGraphics - BGI Demo", 1);
}

void closegraph(void)
{
    if (text_font != NULL)
    {
        if (back_buffer_dc != NULL)
            SelectObject(back_buffer_dc, GetStockObject(SYSTEM_FONT));
        DeleteObject(text_font);
        text_font = NULL;
    }
    if (back_buffer_dc != NULL)
    {
        SelectObject(back_buffer_dc, original_bitmap);
        if (back_buffer_bitmap != NULL)
            DeleteObject(back_buffer_bitmap);
        DeleteDC(back_buffer_dc);
    }
    if (window != NULL)
        DestroyWindow(window);
    window = NULL;
    back_buffer_dc = NULL;
    back_buffer_bitmap = NULL;
    original_bitmap = NULL;
    buffered_key = -1;
    last_error = grNoInitGraph;
}

int graphresult(void) { return last_error; }

const char *grapherrormsg(int errorcode)
{
    switch (errorcode)
    {
    case grOk:
        return "Kein Grafikfehler";
    case grNoInitGraph:
        return "Grafik wurde nicht initialisiert";
    case grNotDetected:
        return "Windows-Grafik konnte nicht initialisiert werden";
    case grInvalidMode:
        return "Ungueltiger Grafikmodus";
    default:
        return "Unbekannter Grafikfehler";
    }
}

int getmaxx(void) { return window_width - 1; }
int getmaxy(void) { return window_height - 1; }
int getx(void) { return current_x; }
int gety(void) { return current_y; }

void setcolor(int color)
{
    if (valid_color(color))
        foreground_color = color;
}

int getcolor(void) { return foreground_color; }

void setbkcolor(int color)
{
    if (valid_color(color))
        background_color = color;
}

int getbkcolor(void) { return background_color; }

void cleardevice(void)
{
    if (!is_ready())
        return;
    clear_dc(back_buffer_dc, window_width, window_height);
    current_x = 0;
    current_y = 0;
}

void putpixel(int x, int y, int color)
{
    if (is_ready() && valid_color(color))
        SetPixelV(back_buffer_dc, x, y, palette[color]);
}

int getpixel(int x, int y)
{
    COLORREF pixel;
    int index;

    if (!is_ready() || x < 0 || y < 0 ||
        x >= window_width || y >= window_height)
        return -1;
    pixel = GetPixel(back_buffer_dc, x, y);
    for (index = 0; index < 16; ++index)
        if (palette[index] == pixel)
            return index;
    return -1;
}

void moveto(int x, int y)
{
    current_x = x;
    current_y = y;
}

void moverel(int dx, int dy) { moveto(current_x + dx, current_y + dy); }

void line(int x1, int y1, int x2, int y2)
{
    HPEN old_pen;
    HPEN pen;

    if (!is_ready())
        return;
    pen = select_pen(back_buffer_dc, foreground_color, &old_pen);
    MoveToEx(back_buffer_dc, x1, y1, NULL);
    LineTo(back_buffer_dc, x2, y2);
    release_pen(back_buffer_dc, pen, old_pen);
    current_x = x2;
    current_y = y2;
}

void lineto(int x, int y) { line(current_x, current_y, x, y); }
void linerel(int dx, int dy) { lineto(current_x + dx, current_y + dy); }

void rectangle(int left, int top, int right, int bottom)
{
    HPEN old_pen;
    HPEN pen;
    HBRUSH old_brush;

    if (!is_ready())
        return;
    ordered_box(&left, &top, &right, &bottom);
    pen = select_pen(back_buffer_dc, foreground_color, &old_pen);
    old_brush = (HBRUSH)SelectObject(back_buffer_dc,
                                     GetStockObject(NULL_BRUSH));
    Rectangle(back_buffer_dc, left, top, right + 1, bottom + 1);
    SelectObject(back_buffer_dc, old_brush);
    release_pen(back_buffer_dc, pen, old_pen);
}

void bar(int left, int top, int right, int bottom)
{
    RECT area;
    HBRUSH brush;

    if (!is_ready())
        return;
    ordered_box(&left, &top, &right, &bottom);
    if (fill_pattern == EMPTY_FILL)
    {
        rectangle(left, top, right, bottom);
        return;
    }
    area.left = left;
    area.top = top;
    area.right = right + 1;
    area.bottom = bottom + 1;
    brush = CreateSolidBrush(palette[fill_color]);
    FillRect(back_buffer_dc, &area, brush);
    DeleteObject(brush);
}

void bar3d(int left, int top, int right, int bottom, int depth, int topflag)
{
    bar(left, top, right, bottom);
    rectangle(left, top, right, bottom);
    line(right, top, right + depth, top - depth);
    line(right, bottom, right + depth, bottom - depth);
    line(right + depth, top - depth, right + depth, bottom - depth);
    if (topflag)
    {
        line(left, top, left + depth, top - depth);
        line(left + depth, top - depth, right + depth, top - depth);
    }
}

static void arc_points(int x, int y, int xradius, int yradius,
                       int angle, int *point_x, int *point_y)
{
    double radians = (double)angle * M_PI / 180.0;
    *point_x = x + (int)(cos(radians) * xradius);
    *point_y = y - (int)(sin(radians) * yradius);
}

void ellipse(int x, int y, int startangle, int endangle,
             int xradius, int yradius)
{
    HPEN old_pen;
    HPEN pen;
    HBRUSH old_brush;
    int start_x;
    int start_y;
    int end_x;
    int end_y;

    if (!is_ready() || xradius < 0 || yradius < 0)
        return;
    pen = select_pen(back_buffer_dc, foreground_color, &old_pen);
    old_brush = (HBRUSH)SelectObject(back_buffer_dc,
                                     GetStockObject(NULL_BRUSH));
    if (endangle - startangle >= 360)
        Ellipse(back_buffer_dc, x - xradius, y - yradius,
                x + xradius + 1, y + yradius + 1);
    else
    {
        arc_points(x, y, xradius, yradius, startangle, &start_x, &start_y);
        arc_points(x, y, xradius, yradius, endangle, &end_x, &end_y);
        Arc(back_buffer_dc, x - xradius, y - yradius,
            x + xradius + 1, y + yradius + 1,
            start_x, start_y, end_x, end_y);
    }
    SelectObject(back_buffer_dc, old_brush);
    release_pen(back_buffer_dc, pen, old_pen);
}

void circle(int x, int y, int radius)
{
    ellipse(x, y, 0, 360, radius, radius);
}

void arc(int x, int y, int startangle, int endangle, int radius)
{
    ellipse(x, y, startangle, endangle, radius, radius);
}

void fillellipse(int x, int y, int xradius, int yradius)
{
    HPEN old_pen;
    HPEN pen;
    HBRUSH old_brush;
    HBRUSH brush;

    if (!is_ready() || xradius < 0 || yradius < 0)
        return;
    pen = select_pen(back_buffer_dc, fill_color, &old_pen);
    brush = CreateSolidBrush(palette[fill_color]);
    old_brush = (HBRUSH)SelectObject(back_buffer_dc, brush);
    Ellipse(back_buffer_dc, x - xradius, y - yradius,
            x + xradius + 1, y + yradius + 1);
    SelectObject(back_buffer_dc, old_brush);
    DeleteObject(brush);
    release_pen(back_buffer_dc, pen, old_pen);
}

void pieslice(int x, int y, int startangle, int endangle, int radius)
{
    HPEN old_pen;
    HPEN pen;
    HBRUSH old_brush;
    HBRUSH brush;
    int start_x;
    int start_y;
    int end_x;
    int end_y;

    if (!is_ready() || radius < 0)
        return;
    arc_points(x, y, radius, radius, startangle, &start_x, &start_y);
    arc_points(x, y, radius, radius, endangle, &end_x, &end_y);
    pen = select_pen(back_buffer_dc, foreground_color, &old_pen);
    brush = CreateSolidBrush(palette[fill_color]);
    old_brush = (HBRUSH)SelectObject(back_buffer_dc, brush);
    Pie(back_buffer_dc, x - radius, y - radius,
        x + radius + 1, y + radius + 1,
        start_x, start_y, end_x, end_y);
    SelectObject(back_buffer_dc, old_brush);
    DeleteObject(brush);
    release_pen(back_buffer_dc, pen, old_pen);
}

void setlinestyle(int linestyle, unsigned int upattern, int thickness)
{
    (void)upattern;
    if (linestyle >= SOLID_LINE && linestyle <= USERBIT_LINE)
        line_style = linestyle;
    line_thickness = thickness > 0 ? thickness : 1;
}

void setfillstyle(int pattern, int color)
{
    if (pattern >= EMPTY_FILL && pattern <= USER_FILL)
        fill_pattern = pattern;
    if (valid_color(color))
        fill_color = color;
}

void settextstyle(int font, int direction, int charsize)
{
    HFONT new_font;
    int height;
    int escapement;

    (void)font;
    if (!is_ready())
        return;
    text_direction = direction == VERT_DIR ? VERT_DIR : HORIZ_DIR;
    text_size = charsize > 0 ? charsize : 1;
    height = text_size >= 2 ? 24 : 16;
    escapement = text_direction == VERT_DIR ? 900 : 0;
    new_font = CreateFontA(-height, 0, escapement, escapement,
                           FW_NORMAL, FALSE, FALSE, FALSE,
                           ANSI_CHARSET, OUT_DEFAULT_PRECIS,
                           CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                           FIXED_PITCH | FF_MODERN, "Consolas");
    if (new_font == NULL)
        return;
    SelectObject(back_buffer_dc, new_font);
    if (text_font != NULL)
        DeleteObject(text_font);
    text_font = new_font;
}

void outtextxy(int x, int y, const char *text)
{
    TEXTMETRICA metrics;
    int draw_y = y;

    if (!is_ready() || text == NULL)
        return;
    SetTextColor(back_buffer_dc, palette[foreground_color]);
    SetBkMode(back_buffer_dc, TRANSPARENT);
    if (text_direction == HORIZ_DIR && GetTextMetricsA(back_buffer_dc, &metrics))
        draw_y = y - metrics.tmAscent;
    TextOutA(back_buffer_dc, x, draw_y, text, (int)strlen(text));
}

void flushgraph(void)
{
    repaint_window();
    pump_events();
}

void flush(void) { flushgraph(); }

int graphkeypressed(void)
{
    pump_events();
    return buffered_key >= 0;
}

int graphgetch(void)
{
    MSG message;
    int key;

    if (back_buffer_dc == NULL)
        return -1;
    while (buffered_key < 0 && window != NULL)
    {
        int result = GetMessageA(&message, NULL, 0, 0);
        if (result <= 0)
            break;
        TranslateMessage(&message);
        DispatchMessageA(&message);
    }
    if (buffered_key < 0)
        return -1;
    key = buffered_key;
    buffered_key = -1;
    return key;
}

void graphdelay(unsigned int milliseconds)
{
    unsigned int remaining = milliseconds;

    while (remaining > 0)
    {
        unsigned int part = remaining > 10 ? 10 : remaining;
        Sleep(part);
        pump_events();
        remaining -= part;
    }
}

#else
#error "vscGraphics unterstuetzt derzeit Linux/X11 und Windows/Win32."
#endif
