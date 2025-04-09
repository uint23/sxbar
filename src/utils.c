#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

void
death(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	vfprintf(stderr, fmt, args);
	va_end(args);

	fprintf(stderr, "\n");
	exit(EXIT_FAILURE);
}
