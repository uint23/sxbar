#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "defs.h"
#include "parser.h"
#include "modules.h"

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
	const char *xdg = getenv("XDG_CONFIG_HOME");
	const char *home = getenv("HOME");
	char path[PATH_MAX];

	if (xdg && *xdg) {
		snprintf(path, sizeof path, "%s/sxbar/sxbarc", xdg);
		if (access(path, R_OK) == 0) {
			return strdup(path);
		}
	}

	if (home && *home) {
		/* new preferred path */
		snprintf(path, sizeof path, "%s/.config/sxbar/sxbarc", home);
		if (access(path, R_OK) == 0) {
			return strdup(path);
		}

		/* legacy path */
		snprintf(path, sizeof path, "%s/.config/sxbarc", home);
		if (access(path, R_OK) == 0) {
			return strdup(path);
		}
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

	int saw_module_key = 0;

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

		/* workspaces.* handling */
		if (!strncmp(key, "workspaces.", 11)) {
			char *field = key + 11;

			if (!strcmp(field, "labels")) {
				/* free previous */
				if (cfg->ws_labels) {
					for (int i = 0; i < cfg->ws_label_count; i++) {
						free(cfg->ws_labels[i]);
					}
					free(cfg->ws_labels);
					cfg->ws_labels = NULL;
					cfg->ws_label_count = 0;
				}
				/* tokenize by spaces */
				char *tmp = strdup(value);
				char *tok = strtok(tmp, " \t");
				while (tok) {
					cfg->ws_labels = realloc(cfg->ws_labels, (cfg->ws_label_count + 1) * sizeof *cfg->ws_labels);
					cfg->ws_labels[cfg->ws_label_count++] = strdup(tok);
					tok = strtok(NULL, " \t");
				}
				free(tmp);
			}
			else if (!strcmp(field, "used_background")) {
				cfg->ws_used_bg = parse_col(value);
			}
			else if (!strcmp(field, "used_foreground")) {
				cfg->ws_used_fg = parse_col(value);
			}
			else if (!strcmp(field, "active_background")) {
				cfg->ws_active_bg = parse_col(value);
			}
			else if (!strcmp(field, "active_foreground")) {
				cfg->ws_active_fg = parse_col(value);
			}
			else if (!strcmp(field, "inactive_background")) {
				cfg->ws_inactive_bg = parse_col(value);
			}
			else if (!strcmp(field, "inactive_foreground")) {
				cfg->ws_inactive_fg = parse_col(value);
			}
			else if (!strcmp(field, "padding_left")) {
				cfg->ws_pad_left = atoi(value);
			}
			else if (!strcmp(field, "padding_right")) {
				cfg->ws_pad_right = atoi(value);
			}
			else if (!strcmp(field, "spacing")) {
				cfg->ws_spacing = atoi(value);
			}
			else if (!strcmp(field, "position")) {
				if (!strcasecmp(value, "left")) {
					cfg->ws_position = WS_POS_LEFT;
				}
				else if (!strcasecmp(value, "center") || !strcasecmp(value, "centre")) {
					cfg->ws_position = WS_POS_CENTER;
				}
				else if (!strcasecmp(value, "right")) {
					cfg->ws_position = WS_POS_RIGHT;
				}
			}
			continue;
		}

		/* module.* handling */
		if (!strncmp(key, "module.", 7)) {
			/* 1st time: discard defaults and start fresh */
			if (!saw_module_key) {
				cleanup_modules();
				cfg->max_modules = 4;
				cfg->modules = calloc(cfg->max_modules, sizeof(Module));
				cfg->module_count = 0;
				saw_module_key = 1;
			}

			char *idx_part = key + 7;
			char *field = strchr(idx_part, '.');
			if (!field) {
				continue;
			}
			*field++ = '\0';
			if (!*idx_part || !*field) {
				continue;
			}
			int idx = atoi(idx_part);
			if (idx < 0) {
				continue;
			}

			/* ensure capacity */
			if (idx >= cfg->max_modules) {
				int new_cap = cfg->max_modules;
				while (idx >= new_cap) {
					new_cap *= 2;
				}
				Module *nm = realloc(cfg->modules, new_cap * sizeof(Module));
				if (!nm) {
					fprintf(stderr, "sxbar: realloc failed for modules\n");
					break;
				}
				/* zero init new slots */
				memset(nm + cfg->max_modules, 0, (new_cap - cfg->max_modules) * sizeof(Module));
				cfg->modules = nm;
				cfg->max_modules = new_cap;
			}
			/* bump count if needed */
			if (idx >= cfg->module_count) {
				for (int i = cfg->module_count; i <= idx; i++) {
					cfg->modules[i].enabled = False;
					cfg->modules[i].refresh_interval = 1;
					cfg->modules[i].last_update = 0;
					cfg->modules[i].cached_output = NULL;
					cfg->modules[i].name = NULL;
					cfg->modules[i].command = NULL;
				}
				cfg->module_count = idx + 1;
			}

			Module *m = &cfg->modules[idx];

			if (!strcmp(field, "name")) {
				free(m->name);
				m->name = strdup(value);
			}
			else if (!strcmp(field, "cmd") || !strcmp(field, "command")) {
				free(m->command);
				m->command = strdup(value);
			}
			else if (!strcmp(field, "enabled")) {
				m->enabled = !strcmp(value, "true") || !strcmp(value, "1") ||
					!strcasecmp(value, "yes") || !strcasecmp(value, "on");
			}
			else if (!strcmp(field, "interval") || !strcmp(field, "refresh_interval")) {
				int iv = atoi(value);
				if (iv > 0) {
					m->refresh_interval = iv;
				}
			}
			continue;
		}

		/* global keys */
		if (!strcmp(key, "bottom_bar")) {
			/* backward compat: map to TOP/BOTTOM if user still uses old key */
			cfg->bar_position = (!strcmp(value, "true") ? BAR_POS_BOTTOM : BAR_POS_TOP);
		} else if (!strcmp(key, "only_used_ws")) {
			cfg->only_used_ws = (!strcmp(value, "true") ? 1 : 0);
        }

		else if (!strcmp(key, "bar_position")) {
			if (!strcasecmp(value, "top")) {
				cfg->bar_position = BAR_POS_TOP;
			}
			else if (!strcasecmp(value, "bottom")) {
				cfg->bar_position = BAR_POS_BOTTOM;
			}
			else if (!strcasecmp(value, "left")) {
				cfg->bar_position = BAR_POS_LEFT;
			}
			else if (!strcasecmp(value, "right")) {
				cfg->bar_position = BAR_POS_RIGHT;
			}
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
		}
		else if (!strcmp(key, "border")) {
			cfg->border = !strcmp(value, "true");
		}
		else if (!strcmp(key, "border_width")) {
			cfg->border_width = atoi(value);
		}
		else if (!strcmp(key, "background_colour")) {
			cfg->background_colour = parse_col(value);
		}
		else if (!strcmp(key, "foreground_colour")) {
			cfg->foreground_colour = parse_col(value);
		}
		else if (!strcmp(key, "border_colour")) {
			cfg->border_colour = parse_col(value);
		}
		else if (!strcmp(key, "font")) {
			free(cfg->font);
			cfg->font = strdup(value);
		}
		else if (!strcmp(key, "font_size")) {
			int sz = atoi(value);
			if (sz > 0 && sz < 512) {
				cfg->font_size = sz;
			}
		}
	}
	fclose(fp);
}
