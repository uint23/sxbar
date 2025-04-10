#include <X11/Xatom.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
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
static void cleanup(void);
static void createwin(void);
static unsigned long getcol(const char *colstr);
static void run(void);
static void setdock(void);
static void sethints(void);
static void setup(void);

// Variables
static unsigned long fgpx;
static unsigned long bgpx;
static unsigned long bdrpx;
static unsigned int	scrw;
static unsigned int	scrh;
static unsigned int	scr;
static unsigned int	barx;
static unsigned int	bary;
static unsigned int	barw;
static Display *dpy;
static Window barwin;
static Window root;

// Functions
static void
createwin(void)
{
    // Create the window
    barwin = XCreateSimpleWindow(
        dpy,
        root,
        barx, bary,
        barw, barh,
        bdrw,
        bdrpx,
        bgpx    
    );

    XSelectInput(dpy, barwin,
        ExposureMask | KeyPressMask
    );
    
    // Set dock properties BEFORE mapping
    setdock();
    
    // Map the window
    XMapWindow(dpy, barwin);

    // Make sure it's on top
    XRaiseWindow(dpy, barwin);
    XSync(dpy, False);
}

static void
cleanup(void)
{
    XDestroyWindow(dpy, barwin);
    XCloseDisplay(dpy);
}

static unsigned long
getcol(const char *colstr)
{
	Colormap cmap = DefaultColormap(dpy, scr);
	XColor col;
	if (XAllocNamedColor(dpy, cmap, colstr, &col, &col) == 0)
		death("Error: can't alloc colour %s", colstr);

	return col.pixel;
}

static void
run(void)
{
	XEvent ev;
	createwin();
	
	for(;;) {
		XNextEvent(dpy, &ev);

		switch (ev.type) {
		case Expose:
			XFlush(dpy);
			break;

		case KeyPress:
			return;
		}
	}
}

static void
setdock(void)
{
    Atom type, dock, state, sticky, strut, strutpartial, override;
    unsigned long strutvals[4] = {0}; // left, right, top, bottom
    
    if (topbar)
        strutvals[2] = barh;
    else
        strutvals[3] = barh;
    
    // Set window type to dock
    type = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
    dock = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DOCK", False);
    XChangeProperty(dpy, barwin, type, XA_ATOM, 32,
                    PropModeReplace, (unsigned char *) &dock, 1);
    
    // Make it sticky
    state = XInternAtom(dpy, "_NET_WM_STATE", False);
    sticky = XInternAtom(dpy, "_NET_WM_STATE_STICKY", False);
    XChangeProperty(dpy, barwin, state, XA_ATOM, 32,
                    PropModeReplace, (unsigned char *) &sticky, 1);
    
    // Set it to appear on all desktops
    Atom desktop = XInternAtom(dpy, "_NET_WM_DESKTOP", False);
    unsigned long alldesktops = 0xFFFFFFFF;
    XChangeProperty(dpy, barwin, desktop, XA_CARDINAL, 32, 
                    PropModeReplace, (unsigned char *)&alldesktops, 1);
    
    // Set basic strut
    strut = XInternAtom(dpy, "_NET_WM_STRUT", False);
    XChangeProperty(dpy, barwin, strut, XA_CARDINAL, 32,
                    PropModeReplace, (unsigned char *)strutvals, 4);
    
    strutpartial = XInternAtom(dpy, "_NET_WM_STRUT_PARTIAL", False);
    unsigned long strutpartialvals[12] = {0};
    memcpy(strutpartialvals, strutvals, 4 * sizeof(unsigned long));
    
    if (topbar) {
        strutpartialvals[8] = 0;
        strutpartialvals[9] = scrw - 1;
    } else {
        strutpartialvals[10] = 0;
        strutpartialvals[11] = scrw - 1;
    }
	XChangeProperty(dpy, barwin, strutpartial, XA_CARDINAL, 32,
			PropModeReplace, (unsigned char *)strutpartialvals, 12);

	override = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_OVERRIDE", False);
	XChangeProperty(dpy, barwin, type, XA_ATOM, 32,
			PropModeAppend, (unsigned char *) &override, 1);

	sethints();
    XSync(dpy, False);
}

static void
sethints(void)
{
	XSizeHints hints;
	hints.flags = PPosition | PSize | PWinGravity;
	hints.x = barx;
	hints.y = bary;
	hints.width = barw;
	hints.height = barh;
	hints.win_gravity = topbar ? NorthWestGravity : SouthWestGravity;
	XSetWMNormalHints(dpy, barwin, &hints);   
}

static void
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

	fgpx = getcol(colfg);
	bgpx = getcol(colbg);
	bdrpx = getcol(colbdr);
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
