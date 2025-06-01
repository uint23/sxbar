#include <X11/X.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "defs.h"
#include "config.h"

static void create_win(void);
static int get_current_workspace(void);
static char **get_workspace_name(int *count);
static void hdl_dummy(XEvent *xev);
static void hdl_expose(XEvent *xev);
static void hdl_property(XEvent *xev);
static ulong parse_col(const char *hex);
static void *run(void *unused);
static void setup(void);
static void update_time(void);
static void update_window_name(void);
static void xev_cases(XEvent *xev);
static int ignore_badwindow(Display *d, XErrorEvent *e);
static int window_exists(Window w);

static EventHandler evtable[LASTEvent];
static XFontStruct *font;
static Display *dpy;
static Window root, win;
static GC gc;
static uint scr;
static ulong fg_col, bg_col, border_col;
static int xerror_occured = 0;

static void create_win(void) {
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

	win = XCreateWindow(dpy, root, x, y, w, h, bw,
		CopyFromParent, InputOutput, DefaultVisual(dpy, scr),
		CWBackPixel | CWBorderPixel | CWEventMask, &wa);

	Atom A_WM_TYPE = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
	Atom A_WM_TYPE_DOCK = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DOCK", False);
	XChangeProperty(dpy, win, A_WM_TYPE, XA_ATOM, 32, PropModeReplace,
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

	XChangeProperty(dpy, win, A_STRUT, XA_CARDINAL, 32, PropModeReplace,
		(unsigned char *)strut, 12);

	XMapRaised(dpy, win);

	gc = XCreateGC(dpy, win, 0, NULL);
	XSetForeground(dpy, gc, fg_col);
	font = XLoadQueryFont(dpy, BAR_FONT);
	if (!font)
		errx(1, "could not load font");
	XSetFont(dpy, gc, font->fid);
}

static int get_current_workspace(void) {
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

static char **get_workspace_name(int *count) {
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

static void hdl_dummy(XEvent *xev) {
	(void)xev;
}

static void hdl_expose(XEvent *xev) {
	(void)xev;
	XClearWindow(dpy, win);
	if (SHOW_TIME) update_time();

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

  if (SHOW_WINDOW_NAME) update_window_name();
}

static void hdl_property(XEvent *xev) {
	Atom net_active = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
	Atom net_current_desktop = XInternAtom(dpy, "_NET_CURRENT_DESKTOP", False);

	if (xev->xproperty.atom == net_current_desktop ||
	    xev->xproperty.atom == net_active) {
		XClearWindow(dpy, win);
		XEvent expose = { .type = Expose };
		evtable[Expose](&expose);
	}
}

static ulong parse_col(const char *hex) {
	XColor col;
	Colormap cmap = DefaultColormap(dpy, scr);

	if (!XParseColor(dpy, cmap, hex, &col)) {
		fprintf(stderr, "sxwm: cannot parse color %s\n", hex);
		return WhitePixel(dpy, scr);
	}
	if (!XAllocColor(dpy, cmap, &col)) {
		fprintf(stderr, "sxwm: cannot allocate color %s\n", hex);
		return WhitePixel(dpy, scr);
	}
	return col.pixel;
}

static void update_time(void) {
	int bar_width = DisplayWidth(dpy, scr) - (BAR_HORI_PAD * 2);
	uint text_y = (BAR_HEIGHT + font->ascent - font->descent) / 2;
	time_t now = time(NULL);
	struct tm *tm = localtime(&now);
	char time_str[64];
	strftime(time_str, sizeof(time_str), "%Y %b %d (%a) %I:%M:%S %p", tm);

	int time_width = XTextWidth(font, time_str, strlen(time_str));
	int time_x = bar_width - time_width - BAR_TEXT_PAD;

	XSetForeground(dpy, gc, bg_col);
	XFillRectangle(dpy, win, gc, time_x, 0, time_width, BAR_HEIGHT);

	XSetForeground(dpy, gc, fg_col);
	XDrawString(dpy, win, gc, time_x, text_y, time_str, strlen(time_str));
	XFlush(dpy);
}

static void *run(void *unused) {
  (void)unused;
	while (1) {
		update_time();
		sleep(1);
	}
	return NULL;
}

static int ignore_badwindow(Display *d, XErrorEvent *e) {
  (void)d;
	if (e->error_code == BadWindow)
		xerror_occured = 1;
	return 0;
}

static int window_exists(Window w) {
	XWindowAttributes attr;
	xerror_occured = 0;

	XErrorHandler old_handler = XSetErrorHandler(ignore_badwindow);
	XGetWindowAttributes(dpy, w, &attr);
	XSync(dpy, False);
	XSetErrorHandler(old_handler);
	return !xerror_occured;
}

static void update_window_name(void) {
	Window active_window = None;
	Atom actual_type;
	int actual_format;
	unsigned long nitems, bytes_after;
	unsigned char *prop = NULL;

	Atom net_active_window = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
	if (XGetWindowProperty(dpy, root, net_active_window, 0, (~0L), False,
		XA_WINDOW, &actual_type, &actual_format,
		&nitems, &bytes_after, &prop) != Success || !prop) {
		return;
	}
	if (nitems == 0) {
		XFree(prop);
		return;
	}

	active_window = *(Window *)prop;
	XFree(prop);

	if (active_window == None || !window_exists(active_window))
		return;

	prop = NULL;
	Atom net_wm_name = XInternAtom(dpy, "_NET_WM_NAME", False);
	Atom utf8 = XInternAtom(dpy, "UTF8_STRING", False);
	if (XGetWindowProperty(dpy, active_window, net_wm_name, 0, (~0L), False,
		utf8, &actual_type, &actual_format,
		&nitems, &bytes_after, &prop) != Success || !prop) {
		return;
	}
	if (nitems == 0) {
		XFree(prop);
		return;
	}

	char *win_name = (char *)prop;
	int win_width = XTextWidth(font, win_name, strlen(win_name));
	int bar_width = DisplayWidth(dpy, scr) - (BAR_HORI_PAD * 2);
	int win_x = (bar_width - win_width) / 2;
	int text_y = (BAR_HEIGHT + font->ascent - font->descent) / 2;

	XSetForeground(dpy, gc, bg_col);
	XFillRectangle(dpy, win, gc, win_x, 0, win_width, BAR_HEIGHT);

	XSetForeground(dpy, gc, fg_col);
	XDrawString(dpy, win, gc, win_x, text_y, win_name, strlen(win_name));
	XFlush(dpy);
	XFree(prop);
}

static void setup(void) {
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

  if (SHOW_TIME)
  {
    pthread_t time_thread;
    if (pthread_create(&time_thread, NULL, run, NULL) != 0)
      errx(1, "Failed to create time update thread");
  }
}

static void xev_cases(XEvent *xev) {
	if (xev->type >= 0 && xev->type < LASTEvent)
		evtable[xev->type](xev);
	else
		printf("sxwm: invalid event type: %d\n", xev->type);
}

int main(int ac, char **av) {
	if (ac > 1) {
		if (!strcmp(av[1], "-v") || !strcmp(av[1], "--version"))
			errx(0, "%s\n%s\n%s", SXBAR_VERSION, SXBAR_AUTHOR, SXBAR_LICINFO);
		else
			errx(0, "usage:\n[-v || --version]: See the version of sxbar");
	}

	setup();

	XEvent xev;
	while (1) {
		XNextEvent(dpy, &xev);
		xev_cases(&xev);
	}
	return 0;
}
