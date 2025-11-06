#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#include "defs.h"
#include "parser.h"

unsigned long parse_col(const char *hex);

static char *skip_spaces(char *s)
{
	while (isspace((unsigned char)*s)) {
		s++;
	}
	if (*s == '\0') {
		return s;
	}
	char *end = s + strlen(s) - 1;
	while (end > s && isspace((unsigned char)*end)) {
		*end-- = '\0';
	}
	return s;
}

char *get_config_path(void)
{
	char path[PATH_MAX];
	const char *home = getenv("HOME");
	if (!home) {
		home = "/tmp"; /* fallback */
	}
	snprintf(path, sizeof path, "%s/.config/sxbarc", home);
	if (access(path, R_OK) == 0) {
		return strdup(path);
	}
	return strdup("/usr/local/share/sxbarc");
}

void parse_config(const char *filepath, Config *cfg)
{
	FILE *fp = fopen(filepath, "r");
	if (!fp) {
		fprintf(stderr, "sxbar: cannot open config %s\n", filepath);
		return;
	}

	char line[256];
	while (fgets(line, sizeof line, fp)) {
		if (!*line || *line == '#') {
			continue;
		}

		char *key = strtok(line, ":");
		char *value = strtok(NULL, "\n");
		if (!key || !value) {
			continue;
		}

		key = skip_spaces(key);
		value = skip_spaces(value);

		if (!strcmp(key, "bottom_bar")) {
			cfg->bottom_bar = !strcmp(value, "true");
		}
		else if (!strcmp(key, "height")) {
			cfg->height = atoi(value);
		}
		else if (!strcmp(key, "vertical_padding")) {
			cfg->vertical_padding = atoi(value);
		}
		else if (!strcmp(key, "horizontal_padding")) {
			cfg->horizontal_padding = atoi(value);
		}
		else if (!strcmp(key, "text_padding")) {
			cfg->text_padding = atoi(value);
		} else if (!strcmp(key, "border")) {
			cfg->border = !strcmp(value, "true");
		}
		else if (!strcmp(key, "border_width")) {
			cfg->border_width = atoi(value);
		} else if (!strcmp(key, "background_colour")) {
			cfg->background_colour = parse_col(value);
		}
		else if (!strcmp(key, "foreground_colour")) {
			cfg->foreground_colour = parse_col(value);
		} else if (!strcmp(key, "border_colour")) {
			cfg->border_colour = parse_col(value);
		}
		else if (!strcmp(key, "font")) {
			free(cfg->font);
			cfg->font = strdup(value);
		}
	}
	fclose(fp);
}
