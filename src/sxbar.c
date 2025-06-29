#define _POSIX_C_SOURCE 200809L
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "defs.h"

void create_win(void);
int get_current_workspace(void);
char** get_workspace_name(int *count);
void hdl_dummy(XEvent *xev);
void hdl_expose(XEvent *xev);
void hdl_property(XEvent *xev);
ulong parse_col(const char *hex);
void run(void);
void setup(void);
void xev_cases(XEvent *xev);

EventHandler evtable[LASTEvent];
XFontStruct *font;
Display *dpy;
Window root, win;
GC gc;
uint scr;
ulong fg_col;
ulong bg_col;
ulong border_col;
#include "config.h"

void create_win(void)
{
	int sw = DisplayWidth(dpy, scr);
	int sh = DisplayHeight(dpy, scr);

	int bw = BAR_BORDER ? BAR_BORDER_W : 0;
	int w = sw - (2 * BAR_HORI_PAD);
	int x = (sw - w) / 2;
	int h = BAR_HEIGHT;
	int y = BOTTOM_BAR ? sh - h - BAR_VERT_PAD - bw : BAR_VERT_PAD;

	XSetWindowAttributes wa = {
		.background_pixel = bg_col,
		.border_pixel = border_col,
		.event_mask = ExposureMask | ButtonPressMask
	};

	win = XCreateWindow(dpy, root,
			x, y,
			w, h,
			bw,
			CopyFromParent,
			InputOutput,
			DefaultVisual(dpy, scr),
			CWBackPixel | CWBorderPixel | CWEventMask,
			&wa);

	Atom A_WM_TYPE = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
	Atom A_WM_TYPE_DOCK = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DOCK", False);
	XChangeProperty(dpy, win, A_WM_TYPE, XA_ATOM, 32,
			PropModeReplace,
			(unsigned char *)&A_WM_TYPE_DOCK, 1);

	Atom A_STRUT = XInternAtom(dpy, "_NET_WM_STRUT_PARTIAL", False);
	long strut[12] = {0};

	if (BOTTOM_BAR) {
		strut[3] = h + bw;
		strut[10] = x;
		strut[11] = x + w - 1;
	} else {
		strut[2] = y + h + bw;
		strut[8] = x;
		strut[9] = x + w - 1;
	}

	XChangeProperty(dpy, win, A_STRUT, XA_CARDINAL, 32,
			PropModeReplace,
			(unsigned char *)strut, 12);

	XMapRaised(dpy, win);

	gc = XCreateGC(dpy, win, 0, NULL);
	XSetForeground(dpy, gc, fg_col);
	font = XLoadQueryFont(dpy, BAR_FONT);
	if (!font)
		errx(1, "could not load font");
	XSetFont(dpy, gc, font->fid);
}

int get_current_workspace(void)
{
	Atom actual_type;
	int actual_format;
	unsigned long nitems, bytes_after;
	unsigned char *data = NULL;
	Atom prop = XInternAtom(dpy, "_NET_CURRENT_DESKTOP", False);

	if (XGetWindowProperty(dpy, root, prop, 0, 1, False,
				XA_CARDINAL, &actual_type, &actual_format,
				&nitems, &bytes_after, &data) == Success && data) {
		int ws = *(unsigned long *)data;
		XFree(data);
		return ws;
	}
	return -1;
}

char** get_workspace_name(int *count)
{
	Atom actual_type;
	int actual_format;
	unsigned long nitems, bytes_after;
	unsigned char *data = NULL;

	Atom prop = XInternAtom(dpy, "_NET_DESKTOP_NAMES", False);
	Atom utf8 = XInternAtom(dpy, "UTF8_STRING", False);

	if (XGetWindowProperty(dpy, root, prop, 0, (~0L), False,
				utf8, &actual_type, &actual_format,
				&nitems, &bytes_after, &data) == Success && data) {

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
	(void) xev;
}

void hdl_expose(XEvent *xev)
{
	(void) xev;
	XClearWindow(dpy, win);

	int current_ws = get_current_workspace();
	int name_count = 0;
	char **names = get_workspace_name(&name_count);

	uint text_y = (BAR_HEIGHT + font->ascent - font->descent) / 2;

	uint text_x = BAR_TEXT_PAD;
	if (names) {
		for (int i = 0; i < name_count; ++i) {
			char label[64];
			if (i == current_ws)
				snprintf(label, sizeof(label), "%s%s%s ",
						BAR_WS_HIGHLIGHT_LEFT, names[i], BAR_WS_HIGHLIGHT_RIGHT);
			else
				snprintf(label, sizeof(label), "%s ", names[i]);

			XDrawString(dpy, win, gc, text_x, text_y,
					label, strlen(label));
			text_x += XTextWidth(font, label, strlen(label)) + BAR_WS_SPACING;
			free(names[i]);
		}
		free(names);
	}

	int bar_width = DisplayWidth(dpy, scr) - (2 * BAR_HORI_PAD);
	int version_width = XTextWidth(font, SXBAR_VERSION, strlen(SXBAR_VERSION));
	int version_x = bar_width - version_width - BAR_TEXT_PAD;

	XDrawString(dpy, win, gc, version_x, text_y,
			SXBAR_VERSION, strlen(SXBAR_VERSION));
}

void hdl_property(XEvent *xev)
{
	if (xev->xproperty.atom == XInternAtom(dpy, "_NET_CURRENT_DESKTOP", False)) {
		XClearWindow(dpy, win);
		XEvent expose;
		expose.type = Expose;
		evtable[Expose](&expose);
	}
}

ulong parse_col(const char *hex)
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
		xev_cases(&xev);
	}
}

void setup(void)
{
	if ((dpy = XOpenDisplay(NULL)) == 0)
		errx(0, "can't open display. quitting...");
	root = XDefaultRootWindow(dpy);
	scr = DefaultScreen(dpy);

	for (int i = 0; i < LASTEvent; ++i)
		evtable[i] = hdl_dummy;

	evtable[Expose] = hdl_expose;
	evtable[PropertyNotify] = hdl_property;

	XSelectInput(dpy, root, PropertyChangeMask);
	fg_col = parse_col(BAR_COLOR_FG);
	bg_col = parse_col(BAR_COLOR_BG);
	border_col = parse_col(BAR_COLOR_BORDER);
	create_win();
}

void xev_cases(XEvent *xev)
{
	if (xev->type >= 0 && xev->type < LASTEvent)
		evtable[xev->type](xev);
	else
		printf("sxwm: invalid event type: %d\n", xev->type);
}

int main(int ac, char **av)
{
	if (ac > 1) {
		if (strcmp(av[1], "-v") == 0 || strcmp(av[1], "--version") == 0)
			errx(0, "%s\n%s\n%s", SXBAR_VERSION, SXBAR_AUTHOR, SXBAR_LICINFO);
		else
			errx(0, "usage:\n[-v || --version]: See the version of sxbar");
	}

	setup();
	run();
	return 0;
}
