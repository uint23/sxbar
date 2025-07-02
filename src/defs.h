#pragma once

#include <X11/Xlib.h>

#define SXBAR_VERSION	"sxbar ver. 1.0"
#define SXBAR_AUTHOR	"(C) Abhinav Prasai 2025"
#define SXBAR_LICINFO	"See LICENSE for more info"

#define MAX_MONITORS 32

typedef struct {
	Bool bottom_bar;
	Bool border;
	unsigned long border_colour;
	unsigned long background_colour;
	unsigned long foreground_colour;
	int height;
	int vertical_padding;
	int horizontal_padding;
	int border_width;
	int text_padding;
	int workspace_spacing;
	char *font;
	char *workspace_highlight_left;
	char *workspace_highlight_right;
} Config;

typedef void (*EventHandler)(XEvent *);
