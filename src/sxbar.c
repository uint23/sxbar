#include <err.h>
#include <string.h>

#include <X11/Xlib.h>

#include "defs.h"

static void create_win(void);
static void setup(void);

static Display *dpy;
static Window root;
static uint scr;

static void
create_win(void)
{
	ulong fg = WhitePixel(dpy, scr);
	ulong bg = BlackPixel(dpy, scr);
}

static void
setup(void)
{
	if ((dpy = XOpenDisplay(NULL)) == 0)
		errx(0, "can't open display. quitting...");
	root = XDefaultRootWindow(dpy);
	scr = DefaultScreen(dpy);
}

int
main(int ac, char **av)
{
	if (ac > 1) {
		if (strcmp(av[1], "-v") == 0 || strcmp(av[1], "--version") == 0)
			errx(0, "%s\n%s\n%s", SXBAR_VERSION, SXBAR_AUTHOR, SXBAR_LICINFO);
		else
			errx(0, "usage:\n[-v || --version]: See the version of sxbar");
	}

	setup();
}
