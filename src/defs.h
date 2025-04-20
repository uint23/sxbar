#ifndef DEFS_H
#define DEFS_H

#include <X11/Xlib.h>

#define uint unsigned int
#define ulong unsigned long
#define u_char unsigned char

#define SXBAR_VERSION	"sxbar ver. 0.1.1"
#define SXBAR_AUTHOR	"(C) Abhinav Prasai 2025"
#define SXBAR_LICINFO	"See LICENSE for more info"

typedef void (*EventHandler)(XEvent *);

#endif
