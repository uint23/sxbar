#pragma once

#include <X11/Xlib.h>

#define SXBAR_VERSION	"sxbar ver. 1.0"
#define SXBAR_AUTHOR	"(C) Abhinav Prasai 2025"
#define SXBAR_LICINFO	"See LICENSE for more info"

typedef struct {
	Bool bottom_bar;
	Bool border;
	long border_colour;
	long background_colour;
	long foreground_colour;
	int height;
	int vertical_padding;
	int horizontal_padding;
	int border_width;
	char *font;
} Config;

typedef void (*EventHandler)(XEvent *);
