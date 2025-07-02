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
void draw_bar_content(Window win, int monitor_index);
int find_window_monitor(Window win);
int get_current_workspace(void);
char **get_workspace_name(int *count);
void hdl_dummy(XEvent *xev);
void hdl_expose(XEvent *xev);
void hdl_property(XEvent *xev);
void init_defaults(void);
unsigned long parse_col(const char *hex);
void run(void);
void setup(void);

EventHandler evtable[LASTEvent];
XFontStruct *font;
Display *dpy;
Window root;
Window *wins = NULL;
int nmonitors = 0;
XineramaScreenInfo *monitors = NULL;
GC gc;
int scr;
Config config;

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

	/* TODO: ADD SUPPORT FOR SELECTING MONITORS */
	wins = malloc(nmonitors * sizeof(Window));

	for (int i = 0; i < nmonitors; i++) {
		int bw = config.border ? config.border_width : 0;
		/* window content width (excluding border) */
		int w = monitors[i].width - (2 * config.horizontal_padding);
		/* padding from screen edge */
		int x = monitors[i].x_org + config.horizontal_padding;
		int h = config.height;
		int y;

		if (config.bottom_bar) {
			y = monitors[i].y_org + monitors[i].height - h - config.vertical_padding - bw;
		}
		else {
			y = monitors[i].y_org + config.vertical_padding;
		}

		XSetWindowAttributes wa = {.background_pixel = config.background_colour,
		                           .border_pixel = config.border_colour,
		                           .event_mask = ExposureMask | ButtonPressMask};

		wins[i] = XCreateWindow(dpy, root, x, y, w, h, bw, CopyFromParent, InputOutput,
		                        DefaultVisual(dpy, scr),
		                        CWBackPixel | CWBorderPixel | CWEventMask, &wa);

		XStoreName(dpy, wins[i], "sxbar");

		XClassHint class_hint;
		class_hint.res_name = "sxbar";
		class_hint.res_class = "sxbar";
		XSetClassHint(dpy, wins[i], &class_hint);

		Atom A_WM_TYPE = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
		Atom A_WM_TYPE_DOCK = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DOCK", False);
		XChangeProperty(dpy, wins[i], A_WM_TYPE, XA_ATOM, 32, PropModeReplace,
		                (unsigned char *)&A_WM_TYPE_DOCK, 1);

		Atom A_STRUT = XInternAtom(dpy, "_NET_WM_STRUT_PARTIAL", False);
		long strut[12] = {0};

		if (config.bottom_bar) {
			strut[3] = h + bw;
			strut[10] = x;
			strut[11] = x + w + (2 * bw) - 1;
		}
		else {
			strut[2] = y + h + bw;
			strut[8] = x;
			strut[9] = x + w + (2 * bw) - 1;
		}

		XChangeProperty(dpy, wins[i], A_STRUT, XA_CARDINAL, 32, PropModeReplace,
		                (unsigned char *)strut, 12);

		XMapRaised(dpy, wins[i]);
	}

	gc = XCreateGC(dpy, wins[0], 0, NULL);
	XSetForeground(dpy, gc, config.foreground_colour);
	font = XLoadQueryFont(dpy, config.font);
	if (!font) {
		errx(1, "could not load font");
	}
	XSetFont(dpy, gc, font->fid);
}

void draw_bar_content(Window win, int monitor_index)
{
	XClearWindow(dpy, win);

	int current_ws = get_current_workspace();
	int name_count = 0;
	char **names = get_workspace_name(&name_count);

	unsigned int text_y = (config.height + font->ascent - font->descent) / 2;
	const int bg_padding = 5;
	const int workspace_spacing = 10;

	if (names) {
		unsigned int *positions = malloc(name_count * sizeof(unsigned int));
		unsigned int *widths = malloc(name_count * sizeof(unsigned int));
		unsigned int text_x = config.text_padding + bg_padding;

		/* calculate consistent positions and widths */
		for (int i = 0; i < name_count; ++i) {
			char padded_label[64];
			snprintf(padded_label, sizeof(padded_label), " %s ", names[i]);

			int label_width = XTextWidth(font, padded_label, strlen(padded_label));

			positions[i] = text_x;
			widths[i] = label_width;

			text_x += label_width + workspace_spacing;
		}

		/* draw backgrounds and text */
		for (int i = 0; i < name_count; ++i) {
			char label[64];
			/* TODO: ADD ABILITY TO CHANGE WORKSPACE STRING */
			snprintf(label, sizeof(label), " %s ", names[i]);

			if (i == current_ws) {
				/* draw highlight background for active workspace */
				/* TODO: ADD OPTION TO CHANGE SPECIFIC HILIGHT COLOUR */
				XSetForeground(dpy, gc, config.foreground_colour);
				XFillRectangle(dpy, win, gc, positions[i] - bg_padding,
				               text_y - font->ascent - bg_padding, widths[i] + (2 * bg_padding),
				               font->ascent + font->descent + (2 * bg_padding));

				/* TODO: ADD CUSTOM TEXT COLOURS */
				XSetForeground(dpy, gc, config.background_colour);
			}
			else {
				XSetForeground(dpy, gc, config.foreground_colour);
			}

			XDrawString(dpy, win, gc, positions[i], text_y, label, strlen(label));
			free(names[i]);
		}

		free(positions);
		free(widths);
		free(names);
	}

	XSetForeground(dpy, gc, config.foreground_colour);

	/* use window content width for version string positioning */
	int version_width = XTextWidth(font, SXBAR_VERSION, strlen(SXBAR_VERSION));
	int window_width = monitors[monitor_index].width - (2 * config.horizontal_padding);
	int version_x = window_width - version_width - config.text_padding - bg_padding;

	XDrawString(dpy, win, gc, version_x, text_y, SXBAR_VERSION, strlen(SXBAR_VERSION));
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
		while (n < MAX_MONITORS && p < (char *)data + nitems) {
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

void hdl_expose(XEvent *xev)
{
	Window win = xev->xexpose.window;
	int monitor_index = find_window_monitor(win);
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


void init_defaults(void)
{
	config.bottom_bar = True;
	config.height = 19;
	config.vertical_padding = 0;
	config.horizontal_padding = 0;
	config.text_padding = 0;
	config.border = False;
	config.border_width = 0;
	config.background_colour = parse_col("#000000");
	config.foreground_colour = parse_col("#7abccd");
	config.border_colour = parse_col("#005577");
	config.font = strdup("fixed");
}

int find_window_monitor(Window win)
{
	for (int i = 0; i < nmonitors; i++) {
		if (wins[i] == win) {
			return i;
		}
	}
	return 0; /* fallback to first monitor */
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

	/* init all events with a dummy */
	for (int i = 0; i < LASTEvent; ++i) {
		evtable[i] = hdl_dummy;
	}

	evtable[Expose] = hdl_expose;
	evtable[PropertyNotify] = hdl_property;

	XSelectInput(dpy, root, PropertyChangeMask);

	init_defaults();
	create_bars();
}

int main(int ac, char **av)
{
	if (ac > 1) {
		if (strcmp(av[1], "-v") == 0 || strcmp(av[1], "--version") == 0) {
			errx(0, "%s\n%s\n%s", SXBAR_VERSION, SXBAR_AUTHOR, SXBAR_LICINFO);
		}
		else {
			errx(0, "usage:\n[-v || --version]: See the version of sxbar");
		}
	}

	setup();
	run();
	return 0;
}
