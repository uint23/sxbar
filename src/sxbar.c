#define _POSIX_C_SOURCE 200809L
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xinerama.h>

#include "defs.h"

void cleanup_modules(void);
void cleanup_resources(void);
void create_bars(void);
static void draw_bar_into(Drawable draw, int monitor_index);
static void redraw_monitor(int monitor_index);
int find_window_monitor(Window win);
int get_current_workspace(void);
char **get_workspace_name(int *count);
void hdl_dummy(XEvent *xev);
void hdl_expose(XEvent *xev);
void hdl_property(XEvent *xev);
void init_defaults(void);
void init_modules(void);
unsigned long parse_col(const char *hex);
void run(void);
char *run_command(const char *cmd);
void setup(void);
void update_modules(void);

EventHandler evtable[LASTEvent];
XFontStruct *font;
Display *dpy;
Window root;
Window *wins = NULL;
XineramaScreenInfo *monitors = NULL;
GC gc;
Config config;
Pixmap *buffers = NULL;
int nmonitors = 0;
int scr;

void cleanup_modules(void)
{
	for (int i = 0; i < config.module_count; i++) {
		free(config.modules[i].name);
		free(config.modules[i].command);
		free(config.modules[i].cached_output);
	}
	free(config.modules);
}

void cleanup_resources(void)
{
	if (buffers) {
		for (int i = 0; i < nmonitors; i++) {
			XFreePixmap(dpy, buffers[i]);
		}
		free(buffers);
	}
	if (wins) {
		for (int i = 0; i < nmonitors; i++) {
			XDestroyWindow(dpy, wins[i]);
		}
		free(wins);
	}
	if (monitors) {
		XFree(monitors);
	}
	if (font) {
		XFreeFont(dpy, font);
	}
	if (gc) {
		XFreeGC(dpy, gc);
	}
	if (dpy) {
		XCloseDisplay(dpy);
	}
}

void create_bars(void)
{
	int xin = 0;
	if (XineramaIsActive(dpy)) {
		monitors = XineramaQueryScreens(dpy, &nmonitors);
		xin = 1;
	}
	if (!xin || nmonitors <= 0) {
		nmonitors = 1;
		monitors = malloc(sizeof *monitors);
		monitors[0].screen_number = 0;
		monitors[0].x_org = 0;
		monitors[0].y_org = 0;
		monitors[0].width = DisplayWidth(dpy, scr);
		monitors[0].height = DisplayHeight(dpy, scr);
	}

	wins = malloc(nmonitors * sizeof *wins);
	buffers = malloc(nmonitors * sizeof *buffers);

	for (int i = 0; i < nmonitors; i++) {
		int bw = config.border ? config.border_width : 0;
		int w = monitors[i].width - 2 * config.horizontal_padding;
		int h = config.height;
		int x = monitors[i].x_org + config.horizontal_padding;
		int y = config.bottom_bar
		            ? monitors[i].y_org + monitors[i].height - h - config.vertical_padding - bw
		            : monitors[i].y_org + config.vertical_padding;

		XSetWindowAttributes wa = {.background_pixel = config.background_colour,
		                           .border_pixel = config.border_colour,
		                           .event_mask = ExposureMask | ButtonPressMask};

		wins[i] = XCreateWindow(dpy, root, x, y, w, h, bw, CopyFromParent, InputOutput,
		                        DefaultVisual(dpy, scr),
		                        CWBackPixel | CWBorderPixel | CWEventMask, &wa);

		XStoreName(dpy, wins[i], "sxbar");
		XClassHint ch = {"sxbar", "sxbar"};
		XSetClassHint(dpy, wins[i], &ch);

		Atom A_WM_TYPE = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
		Atom A_WM_TYPE_DOCK = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DOCK", False);
		XChangeProperty(dpy, wins[i], A_WM_TYPE, XA_ATOM, 32, PropModeReplace,
		                (unsigned char *)&A_WM_TYPE_DOCK, 1);

		Atom A_STRUT = XInternAtom(dpy, "_NET_WM_STRUT_PARTIAL", False);
		long strut[12] = {0};
		if (config.bottom_bar) {
			strut[3] = h + bw + config.vertical_padding;
			strut[10] = x;
			strut[11] = x + w + 2 * bw - 1;
		}
		else {
			strut[2] = y + h + bw;
			strut[8] = x;
			strut[9] = x + w + 2 * bw - 1;
		}
		XChangeProperty(dpy, wins[i], A_STRUT, XA_CARDINAL, 32, PropModeReplace,
		                (unsigned char *)strut, 12);

		buffers[i] = XCreatePixmap(dpy, wins[i], w, h, DefaultDepth(dpy, scr));
		XMapRaised(dpy, wins[i]);
	}

	gc = XCreateGC(dpy, wins[0], 0, NULL);
	XSetForeground(dpy, gc, config.foreground_colour);
	font = XLoadQueryFont(dpy, config.font);
	if (!font) {
		errx(1, "could not load font %s", config.font);
	}
	XSetFont(dpy, gc, font->fid);
}

static void draw_bar_into(Drawable draw, int monitor_index)
{
	int w = monitors[monitor_index].width - 2 * config.horizontal_padding;
	int h = config.height;
	/* clear */
	XSetForeground(dpy, gc, config.background_colour);
	XFillRectangle(dpy, draw, gc, 0, 0, w, h);

	int current_ws = get_current_workspace();
	int name_count = 0;
	char **names = get_workspace_name(&name_count);

	unsigned text_y = (h + font->ascent - font->descent) / 2;
	const int pad = 5, ws_sp = 10, mod_sp = 20;
	unsigned cur_x = config.text_padding + pad;

	/* workspaces */
	if (names) {
		unsigned *pos = malloc(name_count * sizeof *pos);
		unsigned *wd = malloc(name_count * sizeof *wd);
		for (int i = 0; i < name_count; i++) {
			char tmp[64];
			snprintf(tmp, sizeof tmp, " %s ", names[i]);
			wd[i] = XTextWidth(font, tmp, strlen(tmp));
			pos[i] = cur_x;
			cur_x += wd[i] + ws_sp;
		}
		for (int i = 0; i < name_count; i++) {
			char tmp[64];
			snprintf(tmp, sizeof tmp, " %s ", names[i]);
			if (i == current_ws) {
				XSetForeground(dpy, gc, config.foreground_colour);
				XFillRectangle(dpy, draw, gc, pos[i] - pad, text_y - font->ascent - pad,
				               wd[i] + 2 * pad, font->ascent + font->descent + 2 * pad);
				XSetForeground(dpy, gc, config.background_colour);
			}
			else {
				XSetForeground(dpy, gc, config.foreground_colour);
			}
			XDrawString(dpy, draw, gc, pos[i], text_y, tmp, strlen(tmp));
			free(names[i]);
		}
		free(names);
		free(pos);
		free(wd);
	}

	XSetForeground(dpy, gc, config.foreground_colour);

	/* modules */
	int total_mw = 0;
	for (int i = 0; i < config.module_count; i++) {
		if (!config.modules[i].enabled || !config.modules[i].cached_output) {
			continue;
		}
		total_mw += XTextWidth(font, config.modules[i].cached_output,
		                       strlen(config.modules[i].cached_output)) +
		            mod_sp;
	}
	int ver_w = XTextWidth(font, SXBAR_VERSION, strlen(SXBAR_VERSION));
	int mx = w - total_mw - ver_w - 2 * config.text_padding - 2 * pad;

	for (int i = 0; i < config.module_count; i++) {
		if (!config.modules[i].enabled || !config.modules[i].cached_output) {
			continue;
		}
		char *out = config.modules[i].cached_output;
		int tw = XTextWidth(font, out, strlen(out));
		XDrawString(dpy, draw, gc, mx, text_y, out, strlen(out));
		mx += tw + mod_sp;
	}

	/* version */
	int vx = w - ver_w - config.text_padding - pad;
	XDrawString(dpy, draw, gc, vx, text_y, SXBAR_VERSION, strlen(SXBAR_VERSION));
}

static void redraw_monitor(int i)
{
	int w = monitors[i].width - 2 * config.horizontal_padding;
	int h = config.height;
	draw_bar_into(buffers[i], i);
	XCopyArea(dpy, buffers[i], wins[i], gc, 0, 0, w, h, 0, 0);
}

int get_current_workspace(void)
{
	Atom at = XInternAtom(dpy, "_NET_CURRENT_DESKTOP", False);
	Atom ret_type;
	int fmt;
	unsigned long n, after;
	unsigned char *data = NULL;
	if (XGetWindowProperty(dpy, root, at, 0, 1, False, XA_CARDINAL, &ret_type, &fmt, &n, &after,
	                       &data) == Success &&
	    data) {
		int ws = *(unsigned long *)data;
		XFree(data);
		return ws;
	}
	return -1;
}

char **get_workspace_name(int *count)
{
	Atom at = XInternAtom(dpy, "_NET_DESKTOP_NAMES", False);
	Atom utf8 = XInternAtom(dpy, "UTF8_STRING", False);
	Atom ret_type;
	int fmt;
	unsigned long n, after;
	unsigned char *data = NULL;
	if (XGetWindowProperty(dpy, root, at, 0, (~0L), False, utf8, &ret_type, &fmt, &n, &after,
	                       &data) == Success &&
	    data) {
		char **names = NULL;
		int idx = 0;
		char *p = (char *)data;
		while (p < (char *)data + n && idx < MAX_MONITORS) {
			names = realloc(names, (idx + 1) * sizeof *names);
			names[idx++] = strdup(p);
			p += strlen(p) + 1;
		}
		XFree(data);
		*count = idx;
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
	int idx = find_window_monitor(xev->xexpose.window);
	redraw_monitor(idx);
}

void hdl_property(XEvent *xev)
{
	if (xev->xproperty.atom == XInternAtom(dpy, "_NET_CURRENT_DESKTOP", False)) {
		for (int i = 0; i < nmonitors; i++) {
			redraw_monitor(i);
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
	config.font = strdup("terminus-bold-14");
	init_modules();
}

char *skip_spaces(char *s)
{
	while (isspace(*s))
		s++;
	return s;
}

void parse_config(const char *filepath, Config *config)
{
	FILE *config_file;
	config_file = fopen(filepath, "r");

	if (!config_file) {
		fprintf(stderr, "Cannot open config %s\n", filepath);
		return;
	}

	char line[256];

	while (fgets(line, sizeof line, config_file)) {
		if (!*line || *line == '#') {
			continue;
		}

		char *key = strtok(line, ":");
		char *value = strtok(NULL, "\n");

        if (!key || !value) {
            continue;
        }

		key = skip_spaces(key);
		value = skip_spaces(value);

		// printf("%s\n", key);
		// printf("%s\n", value);

        if (!strcmp(key, "bottom_bar")) {
            printf("processing: %s", key);
            config->bottom_bar = *value;
        }
	}
}

int find_window_monitor(Window win)
{
	for (int i = 0; i < nmonitors; i++) {
		if (wins[i] == win) {
			return i;
		}
	}
	return 0;
}

void init_modules(void)
{
	config.max_modules = 10;
	config.modules = malloc(config.max_modules * sizeof(Module));
	config.module_count = 0;

	/* clock */
	config.modules[config.module_count++] = (Module){.name = strdup("clock"),
	                                                 .command = "date '+%H:%M:%S'",
	                                                 .enabled = True,
	                                                 .refresh_interval = 1,
	                                                 .last_update = 0,
	                                                 .cached_output = NULL};
	/* date */
	config.modules[config.module_count++] = (Module){.name = strdup("date"),
	                                                 .command = "date '+%Y-%m-%d'",
	                                                 .enabled = True,
	                                                 .refresh_interval = 60,
	                                                 .last_update = 0,
	                                                 .cached_output = NULL};
	/* battery */
	config.modules[config.module_count++] =
	    (Module){.name = strdup("battery"),
	             .command = "cat /sys/class/power_supply/BAT0/capacity 2>/dev/null | sed "
	                        "'s/$/%/' || echo 'N/A'",
	             .enabled = False,
	             .refresh_interval = 30,
	             .last_update = 0,
	             .cached_output = NULL};
	/* volume */
	config.modules[config.module_count++] =
	    (Module){.name = strdup("volume"),
	             .command = "amixer get Master | grep -o '[0-9]*%' | head -1 || echo 'N/A'",
	             .enabled = True,
	             .refresh_interval = 5,
	             .last_update = 0,
	             .cached_output = NULL};
	/* cpu */
	config.modules[config.module_count++] =
	    (Module){.name = strdup("cpu"),
	             .command = "top -bn1 | grep 'Cpu(s)' | sed 's/.*, *\\([0-9.]*\\)%* id.*/\\1/' "
	                        "| awk '{print 100-$1\"%\"}'",
	             .enabled = True,
	             .refresh_interval = 3,
	             .last_update = 0,
	             .cached_output = NULL};
}

unsigned long parse_col(const char *hex)
{
	XColor col;
	Colormap cmap = DefaultColormap(dpy, scr);
	if (!XParseColor(dpy, cmap, hex, &col) || !XAllocColor(dpy, cmap, &col)) {
		fprintf(stderr, "sxbar: cannot parse/color %s\n", hex);
		return WhitePixel(dpy, scr);
	}
	return col.pixel;
}

void run(void)
{
	XEvent xev;
	time_t last = 0;

	while (True) {
		while (XPending(dpy)) {
			XNextEvent(dpy, &xev);
			evtable[xev.type](&xev);
		}
		time_t now = time(NULL);
		if (now - last >= 1) {
			update_modules();
			for (int i = 0; i < nmonitors; i++) {
				redraw_monitor(i);
			}
			last = now;
		}
		struct timespec ts = {0, 100000000};
		nanosleep(&ts, NULL);
	}
}

char *run_command(const char *cmd)
{
	FILE *fp = popen(cmd, "r");
	if (!fp) {
		return strdup("N/A");
	}

	char buffer[1024];
	char *res = NULL;
	size_t len = 0;

	while (fgets(buffer, sizeof buffer, fp)) {
		size_t l = strlen(buffer);
		if (buffer[l - 1] == '\n') {
			buffer[--l] = '\0';
		}
		if (!res) {
			res = malloc(l + 1);
			strcpy(res, buffer);
			len = l;
		}
		else {
			res = realloc(res, len + l + 2);
			strcat(res, " ");
			strcat(res, buffer);
			len += l + 1;
		}
	}
	pclose(fp);
	return res ? res : strdup("");
}

void update_modules(void)
{
	time_t now = time(NULL);
	for (int i = 0; i < config.module_count; i++) {
		Module *m = &config.modules[i];
		if (!m->enabled) {
			continue;
		}
		if (now - m->last_update >= m->refresh_interval) {
			free(m->cached_output);
			m->cached_output = run_command(m->command);
			m->last_update = now;
		}
	}
}

void setup(void)
{
	if (!(dpy = XOpenDisplay(NULL))) {
		errx(1, "can't open display");
	}
	root = XDefaultRootWindow(dpy);
	scr = DefaultScreen(dpy);

	for (int i = 0; i < LASTEvent; i++) {
		evtable[i] = hdl_dummy;
	}
	evtable[Expose] = hdl_expose;
	evtable[PropertyNotify] = hdl_property;
	XSelectInput(dpy, root, PropertyChangeMask);

	init_defaults();
	parse_config("default_sxbarc", &config);
	create_bars();
}

int main(int ac, char **av)
{
	if (ac > 1) {
		if (!strcmp(av[1], "-v") || !strcmp(av[1], "--version")) {
			printf("%s\n%s\n%s\n", SXBAR_VERSION, SXBAR_AUTHOR, SXBAR_LICINFO);
			return 0;
		}
		errx(1, "usage: sxbar [-v|--version]");
	}
	setup();
	run();

	/* TODO?: never reached */
	cleanup_modules();
	cleanup_resources();
	return 0;
}
