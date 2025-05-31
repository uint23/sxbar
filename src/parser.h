#ifndef PARSER_H
#define PARSER_H

// Config structure to hold parsed values
typedef struct {
    char bg_color[9];      // Including null terminator
    char fg_color[9];
    char border_color[9];
} Config;

// Parse the config file and return a Config struct
Config parse_config_file(const char *path);

#endif // PARSER_H
