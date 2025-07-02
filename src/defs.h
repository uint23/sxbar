#pragma once

#include <X11/Xlib.h>
#include <time.h>

#define SXBAR_VERSION	"sxbar ver. 1.0"
#define SXBAR_AUTHOR	"(C) Abhinav Prasai 2025"
#define SXBAR_LICINFO	"See LICENSE for more info"

#define MAX_MONITORS 32

typedef struct Module {
	char *name;
	char *command;
	int enabled;
	int refresh_interval;
	time_t last_update;
	char *cached_output;
} Module;

typedef struct Config {
	int bottom_bar;
	int height;
	int vertical_padding;
	int horizontal_padding;
	int text_padding;
	int border;
	int border_width;
	unsigned long background_colour;
	unsigned long foreground_colour;
	unsigned long border_colour;
	char *font;
	Module *modules;
	int module_count;
	int max_modules;
} Config;

typedef void (*EventHandler)(XEvent *);
