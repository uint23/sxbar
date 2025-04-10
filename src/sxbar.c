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
static void setup(void);

// Variables
static unsigned int	scrw;
static unsigned int	scrh;
static unsigned int	scr;
static unsigned int	barx;
static unsigned int	bary;
static unsigned int	barw;
static Display 		*dpy;
static Window 		barwin;
static Window		root;

// Functions
static void
createwin(void)
{
	barwin = XCreateSimpleWindow(
		dpy,
		root,
		barx, bary,
		barw, barh,
		bdrw,
		BlackPixel(dpy, scr),
		WhitePixel(dpy, scr)
	);

	XSelectInput(dpy, barwin,
		ExposureMask | KeyPressMask
	);

	XMapWindow(dpy, barwin);
	XRaiseWindow(dpy, barwin);
}

static void
cleanup(void)
{
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

	scr = DefaultScreen(dpy);
	scrw = DisplayWidth(dpy, scr);
	scrh = DisplayHeight(dpy, scr);
	root = RootWindow(dpy, scr);
	
	if (topbar) {
		if (padv) bary = padv;
		else bary = 0;
	} else {
		if (padv) bary = scrh - barh - padv;
		else bary = scrh - barh;
	}

	if (padh) {
		if (centre) {
			barx = padh;
			barw = scrw - (padh * 2);
		} else {
			barx = 0;
			barw = scrw - padh;
		}
	} else {
		barx = 0;
		barw = scrw;
	}

	if (barh > MAXBARH)
		barh = MAXBARH;
	if (padv > MAXPADV)
		padv = MAXPADV;
	if (padh > MAXPADH)
		padh = MAXPADH;
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
