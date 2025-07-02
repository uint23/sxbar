#define _POSIX_C_SOURCE 200809L
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xinerama.h>

#include "defs.h"

void create_bars(void);
int get_current_workspace(void);
char **get_workspace_name(int *count);
void hdl_dummy(XEvent *xev);
void hdl_expose(XEvent *xev);
void hdl_property(XEvent *xev);
unsigned long parse_col(const char *hex);
void run(void);
void setup(void);
int find_monitor_for_window(Window win);
void draw_bar_content(Window win, int monitor_index);

EventHandler evtable[LASTEvent];
XFontStruct *font;
Display *dpy;
Window root;
Window *wins = NULL;
int nmonitors = 0;
XineramaScreenInfo *monitors = NULL;
GC gc;
int scr;
unsigned long fg_col;
unsigned long bg_col;
unsigned long border_col;

#include "config.h"

void create_bars(void)
{
	int xin_active = 0;

	if (XineramaIsActive(dpy)) {
		monitors = XineramaQueryScreens(dpy, &nmonitors);
		xin_active = 1;
	}

	/* fallback to single monitor if Xinerama is not available */
	if (!xin_active || nmonitors <= 0) {
		nmonitors = 1;
		monitors = malloc(sizeof(XineramaScreenInfo));
		monitors[0].screen_number = 0;
		monitors[0].x_org = 0;
		monitors[0].y_org = 0;
		monitors[0].width = DisplayWidth(dpy, scr);
		monitors[0].height = DisplayHeight(dpy, scr);
	}

	wins = malloc(nmonitors * sizeof(Window));

	for (int i = 0; i < nmonitors; i++) {
		int bw = BAR_BORDER ? BAR_BORDER_W : 0;
		int w = monitors[i].width - (2 * BAR_HORI_PAD);
		int x = monitors[i].x_org + (monitors[i].width - w) / 2;
		int h = BAR_HEIGHT;
		int y = BOTTOM_BAR ? monitors[i].y_org + monitors[i].height - h - BAR_VERT_PAD - bw
		                   : monitors[i].y_org + BAR_VERT_PAD;

		XSetWindowAttributes wa = {.background_pixel = bg_col,
		                           .border_pixel = border_col,
		                           .event_mask = ExposureMask | ButtonPressMask};

		wins[i] = XCreateWindow(dpy, root, x, y, w, h, bw, CopyFromParent, InputOutput,
		                        DefaultVisual(dpy, scr),
		                        CWBackPixel | CWBorderPixel | CWEventMask, &wa);

		Atom A_WM_TYPE = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
		Atom A_WM_TYPE_DOCK = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DOCK", False);
		XChangeProperty(dpy, wins[i], A_WM_TYPE, XA_ATOM, 32, PropModeReplace,
		                (unsigned char *)&A_WM_TYPE_DOCK, 1);

		Atom A_STRUT = XInternAtom(dpy, "_NET_WM_STRUT_PARTIAL", False);
		long strut[12] = {0};

		if (BOTTOM_BAR) {
			strut[3] = h + bw;
			strut[10] = x;
			strut[11] = x + w - 1;
		}
		else {
			strut[2] = y + h + bw;
			strut[8] = x;
			strut[9] = x + w - 1;
		}

		XChangeProperty(dpy, wins[i], A_STRUT, XA_CARDINAL, 32, PropModeReplace,
		                (unsigned char *)strut, 12);

		XMapRaised(dpy, wins[i]);
	}

	gc = XCreateGC(dpy, wins[0], 0, NULL);
	XSetForeground(dpy, gc, fg_col);
	font = XLoadQueryFont(dpy, BAR_FONT);
	if (!font) {
		errx(1, "could not load font");
	}
	XSetFont(dpy, gc, font->fid);
}

int get_current_workspace(void)
{
	Atom actual_type;
	int actual_format;
	unsigned long nitems, bytes_after;
	unsigned char *data = NULL;
	Atom prop = XInternAtom(dpy, "_NET_CURRENT_DESKTOP", False);

	if (XGetWindowProperty(dpy, root, prop, 0, 1, False, XA_CARDINAL, &actual_type,
	                       &actual_format, &nitems, &bytes_after, &data) == Success &&
	    data) {
		int ws = *(unsigned long *)data;
		XFree(data);
		return ws;
	}
	return -1;
}

char **get_workspace_name(int *count)
{
	Atom actual_type;
	int actual_format;
	unsigned long nitems, bytes_after;
	unsigned char *data = NULL;

	Atom prop = XInternAtom(dpy, "_NET_DESKTOP_NAMES", False);
	Atom utf8 = XInternAtom(dpy, "UTF8_STRING", False);

	if (XGetWindowProperty(dpy, root, prop, 0, (~0L), False, utf8, &actual_type, &actual_format,
	                       &nitems, &bytes_after, &data) == Success &&
	    data) {

		char **names = NULL;
		int n = 0;
		char *p = (char *)data;
		while (n < 32 && p < (char *)data + nitems) {
			names = realloc(names, (n + 1) * sizeof(char *));
			names[n++] = strdup(p);
			p += strlen(p) + 1;
		}
		XFree(data);
		*count = n;
		return names;
	}
	*count = 0;
	return NULL;
}

void hdl_dummy(XEvent *xev)
{
	(void)xev;
}

int find_monitor_for_window(Window win)
{
	for (int i = 0; i < nmonitors; i++) {
		if (wins[i] == win) {
			return i;
		}
	}
	return 0; /* fallback to first monitor */
}

void draw_bar_content(Window win, int monitor_index)
{
	XClearWindow(dpy, win);

	int current_ws = get_current_workspace();
	int name_count = 0;
	char **names = get_workspace_name(&name_count);

	unsigned int text_y = (BAR_HEIGHT + font->ascent - font->descent) / 2;

	unsigned int text_x = BAR_TEXT_PAD;
	if (names) {
		for (int i = 0; i < name_count; ++i) {
			char label[64];
			if (i == current_ws) {
				snprintf(label, sizeof(label), "%s%s%s ", BAR_WS_HIGHLIGHT_LEFT, names[i],
				         BAR_WS_HIGHLIGHT_RIGHT);
			}
			else {
				snprintf(label, sizeof(label), "%s ", names[i]);
			}

			XDrawString(dpy, win, gc, text_x, text_y, label, strlen(label));
			text_x += XTextWidth(font, label, strlen(label)) + BAR_WS_SPACING;
			free(names[i]);
		}
		free(names);
	}

	int bar_width = monitors[monitor_index].width - (2 * BAR_HORI_PAD);
	int version_width = XTextWidth(font, SXBAR_VERSION, strlen(SXBAR_VERSION));
	int version_x = bar_width - version_width - BAR_TEXT_PAD;

	XDrawString(dpy, win, gc, version_x, text_y, SXBAR_VERSION, strlen(SXBAR_VERSION));
}

void hdl_expose(XEvent *xev)
{
	Window win = xev->xexpose.window;
	int monitor_index = find_monitor_for_window(win);
	draw_bar_content(win, monitor_index);
}

void hdl_property(XEvent *xev)
{
	if (xev->xproperty.atom == XInternAtom(dpy, "_NET_CURRENT_DESKTOP", False)) {
		/* redraw all bars */
		for (int i = 0; i < nmonitors; i++) {
			draw_bar_content(wins[i], i);
		}
	}
}

unsigned long parse_col(const char *hex)
{
	XColor col;
	Colormap cmap = DefaultColormap(dpy, DefaultScreen(dpy));

	if (!XParseColor(dpy, cmap, hex, &col)) {
		fprintf(stderr, "sxwm: cannot parse color %s\n", hex);
		return WhitePixel(dpy, DefaultScreen(dpy));
	}

	if (!XAllocColor(dpy, cmap, &col)) {
		fprintf(stderr, "sxwm: cannot allocate color %s\n", hex);
		return WhitePixel(dpy, DefaultScreen(dpy));
	}

	return col.pixel;
}

void run(void)
{
	XEvent xev;

	for (;;) {
		XNextEvent(dpy, &xev);
		evtable[xev.type](&xev);
	}
}

void setup(void)
{
	if ((dpy = XOpenDisplay(NULL)) == 0) {
		errx(0, "can't open display. quitting...");
	}
	root = XDefaultRootWindow(dpy);
	scr = DefaultScreen(dpy);

	for (int i = 0; i < LASTEvent; ++i) {
		evtable[i] = hdl_dummy;
	}

	evtable[Expose] = hdl_expose;
	evtable[PropertyNotify] = hdl_property;

	XSelectInput(dpy, root, PropertyChangeMask);
	fg_col = parse_col(BAR_COLOR_FG);
	bg_col = parse_col(BAR_COLOR_BG);
	border_col = parse_col(BAR_COLOR_BORDER);
	create_bars();
}

int main(int ac, char **av)
{
	if (ac > 1) {
		if (strcmp(av[1], "-v") == 0 || strcmp(av[1], "--version") == 0)
			errx(0, "%s\n%s\n%s", SXBAR_VERSION, SXBAR_AUTHOR, SXBAR_LICINFO);
		else {
			errx(0, "usage:\n[-v || --version]: See the version of sxbar");
		}
	}

	setup();
	run();
	return 0;
}