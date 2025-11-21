#pragma once

#include <X11/Xlib.h>
#include <time.h>

#define SXBAR_VERSION	"sxbar ver. 1.0"
#define SXBAR_AUTHOR	"(C) Abhinav Prasai 2025"
#define SXBAR_LICINFO	"See LICENSE for more info"

#define MAX_MONITORS 32

#define PATH_MAX 4096

typedef struct Module {
	char *name;
	char *command;
	int enabled;
	int refresh_interval;
	time_t last_update;
	char *cached_output;
} Module;

typedef enum {
	WS_POS_LEFT = 0,
	WS_POS_CENTER = 1,
	WS_POS_RIGHT = 2
} WorkspacePosition;

typedef enum {
	BAR_POS_TOP = 0,
	BAR_POS_BOTTOM = 1,
	BAR_POS_LEFT = 2,
	BAR_POS_RIGHT = 3
} BarPosition;

typedef struct Config {
	BarPosition bar_position;
	int height;
	int vertical_padding;
	int horizontal_padding;
	int text_padding;
	int border;
	int border_width;
	int font_size;
	unsigned long background_colour;
	unsigned long foreground_colour;
	unsigned long border_colour;
	char *font;

	/* modules */
	Module *modules;
	int module_count;
	int max_modules;

	/* workspace customization */
	char **ws_labels;
	int ws_label_count;
	unsigned long ws_active_bg;
	unsigned long ws_active_fg;
	unsigned long ws_inactive_bg;
	unsigned long ws_inactive_fg;
	int ws_pad_left;
	int ws_pad_right;
	int ws_spacing;
	WorkspacePosition ws_position;
} Config;

typedef void (*EventHandler)(XEvent *);
