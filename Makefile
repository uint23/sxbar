CC      ?= gcc
PKG_CONFIG ?= pkg-config

CFLAGS ?= -std=c99 -Os -pipe -Isrc \
          -Wall -Wextra -Wformat=2 -Werror=format-security \
          -Wshadow -Wpointer-arith -Wcast-qual -Wwrite-strings \
          -Wmissing-prototypes -Wstrict-prototypes -Wswitch-enum \
          -Wundef -Wvla -fno-common -fno-strict-aliasing \
          -fstack-protector-strong -fPIE

LDFLAGS ?= -Wl,-Os -pie

# pkg-config libraries
CFLAGS  += $(shell $(PKG_CONFIG) --cflags x11 xinerama xft freetype2)
LDLIBS  += $(shell $(PKG_CONFIG) --libs   x11 xinerama xft freetype2) -lm

PREFIX  ?= /usr/local
BIN     := sxbar
SRC_DIR := src
OBJ_DIR := build
SRC     := $(wildcard $(SRC_DIR)/*.c)
OBJ     := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRC))
DEP     := $(OBJ:.o=.d)

MAN     := sxbar.1
MAN_DIR := $(PREFIX)/share/man/man1

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

-include $(DEP)

$(OBJ_DIR):
	@mkdir -p $@

clean:
	@rm -rf $(OBJ_DIR) $(BIN)

install: all
	@echo "Installing $(BIN) to $(DESTDIR)$(PREFIX)/bin..."
	@mkdir -p "$(DESTDIR)$(PREFIX)/bin"
	@install -m 755 $(BIN) "$(DESTDIR)$(PREFIX)/bin/$(BIN)"
	@echo "Installing man page to $(DESTDIR)$(MAN_DIR)..."
	@mkdir -p $(DESTDIR)$(MAN_DIR)
	@install -m 644 $(MAN) $(DESTDIR)$(MAN_DIR)/
	@echo "Copying default configuration to $(DESTDIR)$(PREFIX)/share/sxbarc..."
	@mkdir -p "$(DESTDIR)$(PREFIX)/share"
	@install -m 644 default_sxbarc "$(DESTDIR)$(PREFIX)/share/sxbarc"
	@echo "Installation complete."

uninstall:
	@echo "Uninstalling $(BIN) from $(DESTDIR)$(PREFIX)/bin..."
	@rm -f "$(DESTDIR)$(PREFIX)/bin/$(BIN)"
	@echo "Uninstalling man page from $(DESTDIR)$(MAN_DIR)..."
	@rm -f $(DESTDIR)$(MAN_DIR)/$(MAN)
	@echo "Uninstallation complete."

clangd:
	@echo "generating compile_flags.txt"
	@rm -f compile_flags.txt
	@for flag in $(CPPFLAGS) $(CFLAGS); do echo $$flag >> compile_flags.txt; done

.PHONY: all clean install uninstall clangd
