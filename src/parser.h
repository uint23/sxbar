#pragma once

#include "defs.h"


char *get_config_path(void);
void parse_config(const char *filepath, Config *cfg);
