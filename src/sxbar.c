#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "config.h"
#include "utils.h"

// Defs
#define VERSION "0.1.0"
#define LICENSE	"This software is under the MIT License.\n\
View the 'LICENSE' file for more info"
#define AUTHOR	"(C) Abhinav Prasai 2025"

// Structs

typedef union {
	
} Args;

typedef struct {
	Window w;
	int tag;
} Client;

// Function declerations
static void createwin(void);
static void cleanup(void);
static void run(void);
static void sethint(void);
static void setup(void);

// Variables
static unsigned int	screenw;
static unsigned int	screenh;
static unsigned int	screen;
static GC			gc;
static Display 		*dpy;
static Window 		barwin, root;

// Functions
static void
debug_config(void)
{
    fprintf(stderr, "Debug Configuration:\n");
    fprintf(stderr, "  screenw: %u\n", screenw);
    fprintf(stderr, "  screenh: %u\n", screenh);
    fprintf(stderr, "  barw: %u\n", barw);
    fprintf(stderr, "  barh: %u\n", barh);
    fprintf(stderr, "  padv: %d\n", padv);
    fprintf(stderr, "  padh: %d\n", padh);
    fprintf(stderr, "  bdrw: %u\n", bdrw);
}

static void
createwin(void)
{
	unsigned int width = (barw > 0) ? barw : 100;
	unsigned int height = (barh > 0) ? barh : 20;
	int x_pos = (padh >= 0) ? padh : 0;
	int y_pos = (padv >= 0) ? padv : 0;
	unsigned int border_width = (bdrw > 0) ? bdrw : 1;

	// Debug
	fprintf(stderr, "Creating window with:\n");
	fprintf(stderr, "  Position: (%d, %d)\n", x_pos, y_pos);
	fprintf(stderr, "  Size: %u x %u\n", width, height);
	fprintf(stderr, "  Border width: %u\n", border_width);

	barwin = XCreateSimpleWindow(
		dpy,
		root,
		padv, padh,
		barw, barh,
		bdrw,
		BlackPixel(dpy, screen),
		WhitePixel(dpy, screen)
	);
	XSelectInput(dpy, barwin,
		ExposureMask | KeyPressMask
	);

	XMapWindow(dpy, barwin);
	sethint();
	gc = XCreateGC(dpy, barwin, 0, NULL);
	XSetForeground(dpy, gc, BlackPixel(dpy, screen));
	XRaiseWindow(dpy, barwin);
}

static void
cleanup(void)
{
    XFreeGC(dpy, gc);
    XDestroyWindow(dpy, barwin);
    XCloseDisplay(dpy);
}

static void
run(void)
{
	XEvent ev;
	createwin();
	
	for(;;) {
		XNextEvent(dpy, &ev);
		if (ev.type == Expose) {
			// Get current time
			time_t now = time(NULL);
			struct tm *tm_info = localtime(&now);
			char time_str[100];
			strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);

			// Add username for demonstration
			char status_text[256];
			snprintf(status_text, sizeof(status_text), "User: uint23 | %s", time_str);

			// Clear the window (redraw the background)
			XClearWindow(dpy, barwin);

			// Draw text
			XDrawString(dpy, barwin, gc, 10, 15, status_text, strlen(status_text));

			// Flush changes to display
			XFlush(dpy);
		} else if (ev.type == KeyPress) {
			return;
		}

		usleep(SECOND(1));
	}
}

void
setup(void)
{
	dpy = XOpenDisplay(NULL);
	if (dpy == 0)
		death("Failed to open display.");

	screen = DefaultScreen(dpy);
	screenw = DisplayWidth(dpy, screen);
	screenh = DisplayHeight(dpy, screen);
	root = RootWindow(dpy, screen);

	if (barw == 0)
		barw = screenw;
	if (barh > screenh)
		barh = screenh;
	if (barw > screenw)
		barw = screenw;
	if (2 * padh >= barw)
		padh = 0;

	barw -= padh * 2;

	if (bdrw > 10)
		bdrw = 1;

	debug_config();
}

void
sethint(void)
{
	XClassHint *classhint = XAllocClassHint();
	if (classhint) {
		classhint->res_name  = "sxbar";
		classhint->res_class = "sxbar";
		XSetClassHint(dpy, barwin, classhint);
		XFree(classhint);
	} else {
		death("Unable to set classhint");
	}
}

int
main(int ac, char **av)
{
	if (ac > 1) {
		if (strcmp(av[1], "-v") == 0 || strcmp(av[1], "--version") == 0)
			death("sxbar ver. %s\n%s\n%s", VERSION, LICENSE, AUTHOR);
		if (strcmp(av[1], "-h") == 0 || strcmp(av[1], "--help") == 0)
			death("USEAGE:\n\t[-v || --version] is the only command..");
		else
			death("USEAGE:\n\t[-v || --version] is the only command..");
	} else {
		setup();
		run();
		cleanup();
	}
	return 0;
}
