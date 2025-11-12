#define _POSIX_C_SOURCE 200809L
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <fontconfig/fontconfig.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xinerama.h>
#include <X11/Xft/Xft.h>

#include "defs.h"
#include "modules.h"

void cleanup_resources(void);
void create_bars(void);
void draw_bar_into(Drawable draw, int monitor_index);
void redraw_monitor(int monitor_index);
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
int xft_center_x(const char *s, int area_w, XftFont *f);
int xft_text_width(const char *s);
int xft_text_adv_v(const char *s);

#include "parser.h"

EventHandler evtable[LASTEvent];
XftFont *font;
XftFont *font_rotated = NULL;
Display *dpy;
Window root;
Window *windows = NULL;
XineramaScreenInfo *monitors = NULL;
GC gc;
Config config;
Pixmap *buffers = NULL;
int n_monitors = 0;
int scr;
XftColor xft_fg, xft_bg;
XftColor xft_ws_inactive_fg;
XftColor xft_ws_active_fg;

void cleanup_resources(void)
{
	if (buffers) {
		for (int i = 0; i < n_monitors; i++) {
			XFreePixmap(dpy, buffers[i]);
		}
		free(buffers);
	}
	if (windows) {
		for (int i = 0; i < n_monitors; i++) {
			XDestroyWindow(dpy, windows[i]);
		}
		free(windows);
	}
	if (monitors) {
		XFree(monitors);
	}

	if (font_rotated && font_rotated != font) {
		XftFontClose(dpy, font_rotated);
	}
	if (font) {
		XftFontClose(dpy, font);
	}

	if (dpy) {
		XftColorFree(dpy, DefaultVisual(dpy, scr), DefaultColormap(dpy, scr), &xft_fg);
		XftColorFree(dpy, DefaultVisual(dpy, scr), DefaultColormap(dpy, scr), &xft_bg);
		XftColorFree(dpy, DefaultVisual(dpy, scr), DefaultColormap(dpy, scr), &xft_ws_inactive_fg);
		XftColorFree(dpy, DefaultVisual(dpy, scr), DefaultColormap(dpy, scr), &xft_ws_active_fg);
	}
	if (gc) {
		XFreeGC(dpy, gc);
	}
	if (dpy) {
		XCloseDisplay(dpy);
	}
	/* free workspace labels */
	if (config.ws_labels) {
		for (int i = 0; i < config.ws_label_count; i++) {
			free(config.ws_labels[i]);
		}
		free(config.ws_labels);
	}
}

void create_bars(void)
{
	int xinerama_active = 0;
	if (XineramaIsActive(dpy)) {
		monitors = XineramaQueryScreens(dpy, &n_monitors);
		xinerama_active = 1;
	}
	if (!xinerama_active || n_monitors <= 0) {
		n_monitors = 1;
		monitors = malloc(sizeof *monitors);
		monitors[0].screen_number = 0;
		monitors[0].x_org = 0;
		monitors[0].y_org = 0;
		monitors[0].width = DisplayWidth(dpy, scr);
		monitors[0].height = DisplayHeight(dpy, scr);
	}

	windows = malloc(n_monitors * sizeof *windows);
	buffers = malloc(n_monitors * sizeof *buffers);

	for (int i = 0; i < n_monitors; i++) {
		int bw = config.border ? config.border_width : 0;

		int w = 0, h = 0, x = 0, y = 0;
		switch (config.bar_position) {
			case BAR_POS_TOP:
				w = monitors[i].width - (2 * config.horizontal_padding) - (2 * bw);
				h = config.height;
				x = monitors[i].x_org + config.horizontal_padding;
				y = monitors[i].y_org + config.vertical_padding;
				break;
			case BAR_POS_BOTTOM:
				w = monitors[i].width - (2 * config.horizontal_padding) - (2 * bw);
				h = config.height;
				x = monitors[i].x_org + config.horizontal_padding;
				y = monitors[i].y_org + monitors[i].height - h - config.vertical_padding - bw;
				break;
			case BAR_POS_LEFT:
				w = config.height;
				h = monitors[i].height - (2 * config.vertical_padding) - (2 * bw);
				x = monitors[i].x_org + config.horizontal_padding;
				y = monitors[i].y_org + config.vertical_padding;
				break;
			case BAR_POS_RIGHT:
				w = config.height;
				h = monitors[i].height - (2 * config.vertical_padding) - (2 * bw);
				x = monitors[i].x_org + monitors[i].width - w - config.horizontal_padding - bw;
				y = monitors[i].y_org + config.vertical_padding;
				break;
			default: /* fallback to bottom */
				w = monitors[i].width - (2 * config.horizontal_padding) - (2 * bw);
				h = config.height;
				x = monitors[i].x_org + config.horizontal_padding;
				y = monitors[i].y_org + monitors[i].height - h - config.vertical_padding - bw;
				break;
		}

		XSetWindowAttributes wa = {
			.background_pixel = config.background_colour,
			.border_pixel = config.border_colour,
			.event_mask = ExposureMask | ButtonPressMask
		};

		windows[i] = XCreateWindow(
				dpy, root, x, y, w, h, bw, CopyFromParent, InputOutput,
				DefaultVisual(dpy, scr),
				CWBackPixel | CWBorderPixel | CWEventMask, &wa
				);

		XStoreName(dpy, windows[i], "sxbar");
		char res_name[] = "sxbar";
		char res_class[] = "sxbar";
		XClassHint ch = {res_name, res_class};
		XSetClassHint(dpy, windows[i], &ch);

		Atom A_WM_TYPE = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
		Atom A_WM_TYPE_DOCK = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DOCK", False);
		XChangeProperty(
				dpy, windows[i], A_WM_TYPE, XA_ATOM, 32, PropModeReplace,
				(unsigned char *)&A_WM_TYPE_DOCK, 1
				);

		Atom A_STRUT = XInternAtom(dpy, "_NET_WM_STRUT_PARTIAL", False);
		long strut[12] = {0};
		switch (config.bar_position) {
			case BAR_POS_BOTTOM:
				strut[3] = h + bw + config.vertical_padding;
				strut[10] = x;
				strut[11] = x + w + 2 * bw - 1;
				break;
			case BAR_POS_TOP:
				strut[2] = y + h + bw;
				strut[8] = x;
				strut[9] = x + w + 2 * bw - 1;
				break;
			case BAR_POS_LEFT:
				strut[0] = w + bw + config.horizontal_padding;
				strut[4] = y;
				strut[5] = y + h + 2 * bw - 1;
				break;
			case BAR_POS_RIGHT:
				strut[1] = w + bw + config.horizontal_padding;
				strut[6] = y;
				strut[7] = y + h + 2 * bw - 1;
				break;
			default: /* fallback to bottom */
				strut[3] = h + bw + config.vertical_padding;
				strut[10] = x;
				strut[11] = x + w + 2 * bw - 1;
				break;
		}
		XChangeProperty(
				dpy, windows[i], A_STRUT, XA_CARDINAL, 32, PropModeReplace,
				(unsigned char *)strut, 12
				);

		buffers[i] = XCreatePixmap(dpy, windows[i], w, h, DefaultDepth(dpy, scr));
		XMapRaised(dpy, windows[i]);
	}

	gc = XCreateGC(dpy, windows[0], 0, NULL);
	XSetForeground(dpy, gc, config.foreground_colour);
	font = NULL;
	if (config.font_size > 0) {
		char spec[256];
		snprintf(spec, sizeof spec, "%s:pixelsize=%d", config.font, config.font_size);
		font = XftFontOpenName(dpy, scr, spec);
	}
	if (!font) {
		font = XftFontOpenName(dpy, scr, config.font);
	}
	if (!font) {
		errx(1, "could not load font %s (size %d)", config.font, config.font_size);
	}

	/* create rotated font for vertical bars */
	if (config.bar_position == BAR_POS_LEFT || config.bar_position == BAR_POS_RIGHT) {
		FcPattern *pat = FcPatternDuplicate(font->pattern);
		if (pat) {
			FcMatrix rot = (FcMatrix){0, 1, -1, 0}; /* 90 deg clockwise */
			FcPatternAddMatrix(pat, FC_MATRIX, &rot);
			FcConfigSubstitute(NULL, pat, FcMatchPattern);
			XftDefaultSubstitute(dpy, scr, pat);
			XftFont *rf = XftFontOpenPattern(dpy, pat);
			if (rf) {
				font_rotated = rf;
				pat = NULL; /* Xft now owns pattern */
			}
			if (pat) {
				FcPatternDestroy(pat);
			}
		}
		if (!font_rotated) {
			font_rotated = font; /* fallback */
		}
	}

	{
		Colormap cmap = DefaultColormap(dpy, scr);
		XColor xcolour;
		XRenderColor render_colour;

		xcolour.pixel = config.foreground_colour;
		XQueryColor(dpy, cmap, &xcolour);
		render_colour.red = xcolour.red;
		render_colour.green = xcolour.green;
		render_colour.blue = xcolour.blue;
		render_colour.alpha = 0xffff;
		if (!XftColorAllocValue(dpy, DefaultVisual(dpy, scr), cmap, &render_colour, &xft_fg)) {
			errx(1, "could not alloc xft fg");
		}

		xcolour.pixel = config.background_colour;
		XQueryColor(dpy, cmap, &xcolour);
		render_colour.red = xcolour.red;
		render_colour.green = xcolour.green;
		render_colour.blue = xcolour.blue;
		render_colour.alpha = 0xffff;
		if (!XftColorAllocValue(dpy, DefaultVisual(dpy, scr), cmap, &render_colour, &xft_bg)) {
			errx(1, "could not alloc xft bg");
		}

		xcolour.pixel = config.ws_inactive_fg;
		XQueryColor(dpy, cmap, &xcolour);
		render_colour.red = xcolour.red;
		render_colour.green = xcolour.green;
		render_colour.blue = xcolour.blue;
		render_colour.alpha = 0xffff;
		if (!XftColorAllocValue(dpy, DefaultVisual(dpy, scr), cmap, &render_colour, &xft_ws_inactive_fg)) {
			errx(1, "could not alloc xft ws inactive fg");
		}

		xcolour.pixel = config.ws_active_fg;
		XQueryColor(dpy, cmap, &xcolour);
		render_colour.red = xcolour.red;
		render_colour.green = xcolour.green;
		render_colour.blue = xcolour.blue;
		render_colour.alpha = 0xffff;
		if (!XftColorAllocValue(dpy, DefaultVisual(dpy, scr), cmap, &render_colour, &xft_ws_active_fg)) {
			errx(1, "could not alloc xft ws active fg");
		}
	}
}

void draw_bar_into(Drawable draw, int monitor_index)
{
	int w, h;
	if (config.bar_position == BAR_POS_LEFT || config.bar_position == BAR_POS_RIGHT) {
		int bw = config.border ? config.border_width : 0;
		w = config.height;
		h = monitors[monitor_index].height - 2 * config.vertical_padding - 2 * bw;
	}
	else {
		int bw = config.border ? config.border_width : 0;
		w = monitors[monitor_index].width - 2 * config.horizontal_padding - 2 * bw;
		h = config.height;
	}

	XSetForeground(dpy, gc, config.background_colour);
	XFillRectangle(dpy, draw, gc, 0, 0, w, h);
	XftDraw *xd = XftDrawCreate(dpy, draw, DefaultVisual(dpy, scr), DefaultColormap(dpy, scr));

	/* vertical bar special path */
	if (config.bar_position == BAR_POS_LEFT || config.bar_position == BAR_POS_RIGHT) {
		XftFont *vf = font_rotated ? font_rotated : font;

		int current_ws = get_current_workspace();
		int name_count = 0;
		char **names = get_workspace_name(&name_count);

		char **labels = NULL;
		int label_count = 0;
		if (config.ws_labels && config.ws_label_count > 0) {
			labels = config.ws_labels;
			label_count = config.ws_label_count;
		}
		else if (names) {
			labels = names;
			label_count = name_count;
		}

		/* measure modules total vertical advance */
		int modules_total_adv = 0;
		for (int i = 0; i < config.module_count; i++) {
			if (!config.modules[i].enabled || !config.modules[i].cached_output) {
				continue;
			}
			modules_total_adv += xft_text_adv_v(config.modules[i].cached_output) + 20;
		}

		int ws_segment_adv = 0;
		if (labels) {
			for (int i = 0; i < label_count; i++) {
				int adv = xft_text_adv_v(labels[i]);
				ws_segment_adv += adv + config.ws_pad_left + config.ws_pad_right;
				if (i + 1 < label_count) {
					ws_segment_adv += config.ws_spacing;
				}
			}
		}

		int modules_block_top = h - modules_total_adv - 2 * config.text_padding;
		int ws_start_y;
		switch (config.ws_position) {
			case WS_POS_CENTER:
				ws_start_y = (h - ws_segment_adv) / 2;
				break;
			case WS_POS_RIGHT:
				ws_start_y = modules_block_top - ws_segment_adv - config.ws_spacing;
				if (ws_start_y < 0) {
					ws_start_y = 0;
				}
				break;
			case WS_POS_LEFT:
			default:
				ws_start_y = config.text_padding;
				break;
		}

		/* workspaces */
		if (labels) {
			int cur_y = ws_start_y;
			for (int i = 0; i < label_count; i++) {
				const char *txt = labels[i];
				int adv = xft_text_adv_v(txt);
				int box_adv = adv + config.ws_pad_left + config.ws_pad_right;

				/* background rectangle */
				XSetForeground(dpy, gc, (i == current_ws) ? config.ws_active_bg : config.ws_inactive_bg);
				XFillRectangle(dpy, draw, gc, 0, cur_y, w, box_adv);
				XftColor *col = (i == current_ws) ? &xft_ws_active_fg : &xft_ws_inactive_fg;

				/* draw rotated text centered horizontally within bar */
				int text_offset = cur_y + config.ws_pad_left + vf->ascent;
				int cx = xft_center_x(txt, w, vf);
				XftDrawStringUtf8(xd, col, vf, cx, text_offset,
						(const FcChar8 *)txt, strlen(txt));

				cur_y += box_adv + config.ws_spacing;
			}
		}

		if (names) {
			for (int i = 0; i < name_count; i++) {
				free(names[i]);
			}
			free(names);
		}

		/* modules */
		int my = h - modules_total_adv - 2 * config.text_padding;
		for (int i = 0; i < config.module_count; i++) {
			if (!config.modules[i].enabled || !config.modules[i].cached_output) {
				continue;
			}
			char *out = config.modules[i].cached_output;
			int adv = xft_text_adv_v(out);

			int cx = xft_center_x(out, w, vf);
			XftDrawStringUtf8(xd, &xft_fg, vf, cx, my + vf->ascent,
					(const FcChar8 *)out, strlen(out));
			my += adv + 20;
		}

		XftDrawDestroy(xd);
		return;
	}

	int current_ws = get_current_workspace();
	int name_count = 0;
	char **names = get_workspace_name(&name_count);

	/* choose label source */
	char **labels = NULL;
	int label_count = 0;
	if (config.ws_labels && config.ws_label_count > 0) {
		labels = config.ws_labels;
		label_count = config.ws_label_count;
	}
	else if (names) {
		labels = names;
		label_count = name_count;
	}

	unsigned text_y = (h + font->ascent - font->descent) / 2;

	/* measure workspace segment width */
	int ws_segment_width = 0;
	if (labels) {
		for (int i = 0; i < label_count; i++) {
			char tmp[128];
			snprintf(tmp, sizeof tmp, "%s", labels[i]);
			int tw = xft_text_width(tmp);
			ws_segment_width += tw + config.ws_pad_left + config.ws_pad_right;
			if (i + 1 < label_count) {
				ws_segment_width += config.ws_spacing;
			}
		}
	}

	/* compute starting x for workspaces based on position */
	int modules_total_w = 0;
	for (int i = 0; i < config.module_count; i++) {
		if (!config.modules[i].enabled || !config.modules[i].cached_output) {
			continue;
		}
		modules_total_w += xft_text_width(config.modules[i].cached_output) + 20;
	}
	int modules_block_left = w - modules_total_w - 2 * config.text_padding;

	int ws_start_x;
	switch (config.ws_position) {
		case WS_POS_CENTER:
			ws_start_x = (w - ws_segment_width) / 2;
			break;
		case WS_POS_RIGHT:
			/* keep space for modules on far right */
			ws_start_x = modules_block_left - ws_segment_width - config.ws_spacing;
			if (ws_start_x < 0) {
				ws_start_x = 0;
			}
			break;
		case WS_POS_LEFT:
		default:
			ws_start_x = config.text_padding;
			break;
	}

	/* draw workspaces */
	if (labels) {
		int cur_x = ws_start_x;
		for (int i = 0; i < label_count; i++) {
			char tmp[128];
			snprintf(tmp, sizeof tmp, "%s", labels[i]);
			int tw = xft_text_width(tmp);
			int box_w = tw + config.ws_pad_left + config.ws_pad_right;
			unsigned box_x = cur_x;
			unsigned box_y = text_y - font->ascent - config.ws_pad_left;
			unsigned box_h = font->ascent + font->descent + config.ws_pad_left + config.ws_pad_right;

			/* background */
			XSetForeground(dpy, gc, (i == current_ws) ? config.ws_active_bg : config.ws_inactive_bg);
			XFillRectangle(dpy, draw, gc, box_x, box_y, box_w, box_h);

			/* text */
			if (i == current_ws) {
				XftDrawStringUtf8(
					xd, &xft_ws_active_fg, font,
					box_x + config.ws_pad_left, text_y,
					(const FcChar8 *)tmp, strlen(tmp)
				);
			}
			else {
				XftDrawStringUtf8(
					xd, &xft_ws_inactive_fg, font,
					box_x + config.ws_pad_left, text_y,
					(const FcChar8 *)tmp, strlen(tmp)
				);
			}

			cur_x += box_w + config.ws_spacing;
		}
	}

	/* free EWMH names if used */
	if (names) {
		for (int i = 0; i < name_count; i++) {
			free(names[i]);
		}
		free(names);
	}

	/* modules */
	int mx = w - modules_total_w - 2 * config.text_padding;
	for (int i = 0; i < config.module_count; i++) {
		if (!config.modules[i].enabled || !config.modules[i].cached_output) {
			continue;
		}
		char *out = config.modules[i].cached_output;
		int tw = xft_text_width(out);
		XftDrawStringUtf8(xd, &xft_fg, font, mx, text_y, (const FcChar8 *)out, strlen(out));
		mx += tw + 20;
	}

	XftDrawDestroy(xd);
}

void redraw_monitor(int i)
{
	int w, h;
	if (config.bar_position == BAR_POS_LEFT || config.bar_position == BAR_POS_RIGHT) {
		int bw = config.border ? config.border_width : 0;
		w = config.height;
		h = monitors[i].height - 2 * config.vertical_padding - 2 * bw;
	}
	else {
		int bw = config.border ? config.border_width : 0;
		w = monitors[i].width - 2 * config.horizontal_padding - 2 * bw;
		h = config.height;
	}
	draw_bar_into(buffers[i], i);
	XCopyArea(dpy, buffers[i], windows[i], gc, 0, 0, w, h, 0, 0);
}

int get_current_workspace(void)
{
	Atom at = XInternAtom(dpy, "_NET_CURRENT_DESKTOP", False);
	Atom ret_type;
	int fmt;
	unsigned long n, after;
	unsigned char *data = NULL;
	if (XGetWindowProperty(dpy, root, at, 0, 1, False, XA_CARDINAL,
		&ret_type, &fmt, &n, &after, &data) == Success && data) {
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
	if (XGetWindowProperty(dpy, root, at, 0, (~0L), False, utf8,
		&ret_type, &fmt, &n, &after, &data) == Success && data) {
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
		for (int i = 0; i < n_monitors; i++) {
			redraw_monitor(i);
		}
	}
}

void init_defaults(void)
{
	config.bar_position = BAR_POS_BOTTOM;
	config.height = 19;
	config.vertical_padding = 0;
	config.horizontal_padding = 0;
	config.text_padding = 0;
	config.border = False;
	config.border_width = 0;
	config.background_colour = parse_col("#000000");
	config.foreground_colour = parse_col("#7abccd");
	config.border_colour = parse_col("#005577");
	config.font = strdup("monospace");
	config.font_size = 0;

	/* modules */
	config.modules = NULL;
	config.module_count = 0;
	config.max_modules = 0;

	/* workspace customization defaults */
	config.ws_labels = NULL;
	config.ws_label_count = 0;
	config.ws_active_bg = config.foreground_colour;
	config.ws_active_fg = config.background_colour;
	config.ws_inactive_bg = config.background_colour;
	config.ws_inactive_fg = config.foreground_colour;
	config.ws_pad_left = 5;
	config.ws_pad_right = 5;
	config.ws_spacing = 10;
	config.ws_position = WS_POS_LEFT;
}


int find_window_monitor(Window win)
{
	for (int i = 0; i < n_monitors; i++) {
		if (windows[i] == win) {
			return i;
		}
	}
	return 0;
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
			for (int i = 0; i < n_monitors; i++) {
				redraw_monitor(i);
			}
			last = now;
		}
		struct timespec ts = {0, 100000000};
		nanosleep(&ts, NULL);
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
	char *cfgpath = get_config_path();
	parse_config(cfgpath, &config);
	free(cfgpath);
	create_bars();
}

int xft_center_x(const char *s, int area_w, XftFont *f)
{
	XGlyphInfo ext;
	XftTextExtentsUtf8(dpy, f, (const FcChar8 *)s, strlen(s), &ext);

	int avail = area_w - 2 * config.text_padding;
	if (avail < 0) {
		avail = 0;
	}

	int cx = config.text_padding + ((avail - (int)ext.width) / 2) - (int)ext.x;
	if (cx < 0) {
		cx = 0;
	}
	return cx;
}

int xft_text_width(const char *s)
{
	XGlyphInfo ext;
	XftTextExtentsUtf8(dpy, font, (const FcChar8 *)s, strlen(s), &ext);

	int ink_right = ext.x + (int)ext.width;
	return ink_right > (int)ext.xOff ? ink_right : (int)ext.xOff;
}

int xft_text_adv_v(const char *s)
{
	XGlyphInfo ext;
	XftFont *f = font_rotated ? font_rotated : font;
	XftTextExtentsUtf8(dpy, f, (const FcChar8 *)s, strlen(s), &ext);

	int adv = ext.yOff;
	if (adv < 0) {
		adv = -adv;
	}

	int ink = ext.y + (int)ext.height;
	if (ink > adv) {
		adv = ink;
	}

	return adv;
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
	cleanup_resources();
	return 0;
}
