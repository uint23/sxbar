#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "parser.h"
#include "config.h"

#define MAX_LINE_LENGTH 256

static void trim(char *str) {
    char *start = str;
    while(isspace(*start)) start++;
    
    if(start != str) {
        memmove(str, start, strlen(start) + 1);
    }

    char *end = str + strlen(str) - 1;
    while(end > str && isspace(*end)) {
        *end = '\0';
        end--;
    }
}

Config parse_config_file(const char *path) {
    Config config;
    // Set defaults from config.h
    strncpy(config.bg_color, BAR_COLOR_BG, sizeof(config.bg_color) - 1);
    strncpy(config.fg_color, BAR_COLOR_FG, sizeof(config.fg_color) - 1);
    strncpy(config.border_color, BAR_COLOR_BORDER, sizeof(config.border_color) - 1);

    FILE *file = fopen(path, "r");
    if (!file) {
        return config; // Return default config if file doesn't exist
    }

    char line[MAX_LINE_LENGTH];
    while (fgets(line, sizeof(line), file)) {
        // Remove newline
        line[strcspn(line, "\n")] = 0;
        
        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\0') {
            continue;
        }

        char *key = strtok(line, ":");
        char *value = strtok(NULL, "\n");
        
        if (!key || !value) {
            continue;
        }

        // Trim whitespace
        trim(key);
        trim(value);

        // Parse the values
        if (strcmp(key, "background_color") == 0) {
            strncpy(config.bg_color, value, sizeof(config.bg_color) - 1);
        } else if (strcmp(key, "foreground_color") == 0) {
            strncpy(config.fg_color, value, sizeof(config.fg_color) - 1);
        } else if (strcmp(key, "border_color") == 0) {
            strncpy(config.border_color, value, sizeof(config.border_color) - 1);
        }
    }

    fclose(file);
    return config;
}
