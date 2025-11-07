#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "defs.h"
#include "modules.h"

extern Config config;

static char *run_command(const char *cmd)
{
	if (!cmd || !*cmd) {
		return strdup("");
	}

	FILE *fp = popen(cmd, "r");
	if (!fp) {
		return strdup("N/A");
	}

	char buffer[1024];
	char *res = NULL;
	size_t len = 0;

	while (fgets(buffer, sizeof buffer, fp)) {
		size_t l = strlen(buffer);
		if (l > 0 && buffer[l - 1] == '\n') {
			buffer[--l] = '\0';
		}

		if (!res) {
			res = malloc(l + 1);
			if (!res) {
				break;
			}
			strcpy(res, buffer);
			len = l;
		}
		else {
			char *tmp = realloc(res, len + l + 2);
			if (!tmp) {
				break;
			}
			res = tmp;
			strcat(res, " ");
			strcat(res, buffer);
			len += l + 1;
		}
	}
	pclose(fp);
	return res ? res : strdup("");
}

void cleanup_modules(void)
{
	for (int i = 0; i < config.module_count; i++) {
		free(config.modules[i].name);
		free(config.modules[i].command);
		free(config.modules[i].cached_output);
	}
	free(config.modules);
	config.modules = NULL;
	config.module_count = 0;
	config.max_modules = 0;
}

void update_modules(void)
{
	time_t now = time(NULL);
	for (int i = 0; i < config.module_count; i++) {
		Module *m = &config.modules[i];

		if (!m->enabled) {
			continue;
		}

		if (m->refresh_interval <= 0) {
			m->refresh_interval = 1;
		}

		if (now - m->last_update >= m->refresh_interval) {
			free(m->cached_output);
			m->cached_output = run_command(m->command);
			m->last_update = now;
		}
	}
}
