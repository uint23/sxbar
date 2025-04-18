// MIT License
// See LICENSE for more information
// Config file - see, it's not that difficult

#define MAXPADV	500	// The vertical padding of the bar,
#define MAXPADH	500	// The horizonral padding of the bar,
#define MAXBARH	40	// The max height of the bar, this shouldn't really be more than 40
#define MAXBDRW	10	// The max border width of the bar

#include <X11/keysym.h>
#define BUTTONLC 1 	// Left click
#define BUTTONMC 2 	// Middle click
#define BUTTONRC 3 	// Right click

// Keybindings
//#define TOGGLE	{ ModMask|ShiftMask,	XK_b,	{.v=toggle}}

static unsigned int	topbar	= 1;	// Bar is at top (1) or bottom (0)
static unsigned int	centre	= 1;	// If using padding, this will centre your bar
static unsigned int	barh	= 20;	// Bar height
static unsigned int	padh	= 400;	// Horizontal bar padding
static unsigned int	padv	= 10;	// Vertical bar padding
static unsigned int bdrw	= 10;	// Border width of the bar. Set to 0 for no border

static const char colbg[]	= "#A0A0DD";	// Background colour
static const char colbdr[]	= "#FF0000";	// Border colour
// static const char *font		= "terminus";
static const char *tags[]	= { "1", "2", "3", "4", "5", "6", "7", "8", "9" };
