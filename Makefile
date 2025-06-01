CC = gcc
CFLAGS = -Wall -Wextra -O3 -g -Isrc -march=native -flto -s -Os
LDFLAGS = -lX11

SRC_DIR = src
SRC = $(wildcard $(SRC_DIR)/*.c)
OBJ = $(SRC:.c=.o)
BIN = sxbar
PREFIX = /usr/local
MANDIR = /usr/local/share/man
MAN1DIR = $(MANDIR)/man1

# Man page for sxbar
MANPAGE = sxbar.1

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

$(SRC_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(SRC_DIR)/*.o $(BIN)

install: all
	@echo "Installing $(BIN) to $(DESTDIR)$(PREFIX)/bin..."
	@mkdir -p $(DESTDIR)$(PREFIX)/bin
	@install -m 755 $(BIN) $(DESTDIR)$(PREFIX)/bin/$(BIN)
	@echo "Installation complete."

	@echo "Installing man page to $(DESTDIR)$(MAN1DIR)..."
	@mkdir -p $(DESTDIR)$(MAN1DIR)
	@install -m 644 $(MANPAGE) $(DESTDIR)$(MAN1DIR)/$(MANPAGE)
	@echo "Man page installation complete."

uninstall:
	@echo "Uninstalling $(BIN) from $(DESTDIR)$(PREFIX)/bin..."
	@rm -f $(DESTDIR)$(PREFIX)/bin/$(BIN)
	@echo "Uninstallation complete."

	@echo "Uninstalling man page from $(DESTDIR)$(MAN1DIR)..."
	@rm -f $(DESTDIR)$(MAN1DIR)/$(MANPAGE)
	@echo "Man page uninstallation complete."

clean-install: clean install

.PHONY: all clean install uninstall clean-install
