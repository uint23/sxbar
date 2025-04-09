# Flags
CC = gcc
CFLAGS = -Wall -O2 -g `pkg-config --cflags x11 xft`
LDFLAGS = `pkg-config --libs x11 xft`

# Dirs
SRC_DIR = src
OBJ_DIR = obj
BIN = sxbar

# Source and object files
SRC = $(wildcard $(SRC_DIR)/*.c)
OBJ = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRC))

.PHONY: all clean

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(BIN)
