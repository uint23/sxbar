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
			strcpy(res, buffer);
			len = l;
		}
		else {
			res = realloc(res, len + l + 2);
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

void init_modules(void)
{
	config.max_modules = 10;
	config.modules = malloc(config.max_modules * sizeof(Module));
	config.module_count = 0;

	/* clock */
	config.modules[config.module_count++] = (Module){
		.name = strdup("clock"),
		.command = strdup("date '+%H:%M:%S'"),
		.enabled = True,
		.refresh_interval = 1,
		.last_update = 0,
		.cached_output = NULL
	};

	/* date */
	config.modules[config.module_count++] = (Module){
		.name = strdup("date"),
		.command = strdup("date '+%Y-%m-%d'"),
		.enabled = True,
		.refresh_interval = 60,
		.last_update = 0,
		.cached_output = NULL
	};

	/* battery */
	config.modules[config.module_count++] = (Module){
		.name = strdup("battery"),
		.command = strdup("cat /sys/class/power_supply/BAT0/capacity 2>/dev/null | sed 's/$/%/' || echo 'N/A'"),
		.enabled = False,
		.refresh_interval = 30,
		.last_update = 0,
		.cached_output = NULL
	};

	/* volume */
	config.modules[config.module_count++] = (Module){
		.name = strdup("volume"),
		.command = strdup("amixer get Master | grep -o '[0-9]*%' | head -1 || echo 'N/A'"),
		.enabled = True,
		.refresh_interval = 5,
		.last_update = 0,
		.cached_output = NULL
	};

	/* cpu */
	config.modules[config.module_count++] = (Module){
		.name = strdup("cpu"),
		.command = strdup("top -bn1 | grep 'Cpu(s)' | sed 's/.*, *\\([0-9.]*\\)%* id.*/\\1/' | awk '{print 100-$1\"%\"}'"),
		.enabled = True,
		.refresh_interval = 3,
		.last_update = 0,
		.cached_output = NULL
	};
}

void update_modules(void)
{
	time_t now = time(NULL);
	for (int i = 0; i < config.module_count; i++) {
		Module *m = &config.modules[i];
		if (!m->enabled) {
			continue;
		}
		if (now - m->last_update >= m->refresh_interval) {
			free(m->cached_output);
			m->cached_output = run_command(m->command);
			m->last_update = now;
		}
	}
}
