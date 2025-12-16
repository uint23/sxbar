# tools
CC = cc

# paths
PREFIX = /usr/local
MANPREFIX = ${PREFIX}/share/man

# libs
LIBS = -lX11 -lXinerama -lXft -lfontconfig -lfreetype -lm


# flags
CPPFLAGS = -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=700
CFLAGS = -std=c99 -pedantic -Wall -Wextra -Os ${CPPFLAGS} -I/usr/X11R6/include -I/usr/X11R6/include/freetype2 -I/usr/include/freetype2
LDFLAGS = ${LIBS} -L/usr/X11R6/lib

# files
SRC = src/sxbar.c src/modules.c src/parser.c
OBJ = build/sxbar.o build/modules.o build/parser.o
BIN = sxbar

all: ${BIN}

# rules
build/sxbar.o: src/sxbar.c src/defs.h src/modules.h src/parser.h
	mkdir -p build
	${CC} -c ${CFLAGS} src/sxbar.c -o build/sxbar.o

build/modules.o: src/modules.c src/modules.h src/defs.h
	mkdir -p build
	${CC} -c ${CFLAGS} src/modules.c -o build/modules.o

build/parser.o: src/parser.c src/parser.h src/defs.h
	mkdir -p build
	${CC} -c ${CFLAGS} src/parser.c -o build/parser.o

${BIN}: ${OBJ}
	${CC} -o ${BIN} ${OBJ} ${LDFLAGS}

clean:
	rm -rf build ${BIN}

install: all
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f ${BIN} ${DESTDIR}${PREFIX}/bin/
	chmod 755 ${DESTDIR}${PREFIX}/bin/${BIN}

	mkdir -p ${DESTDIR}${MANPREFIX}/man1
	cp -f sxbar.1 ${DESTDIR}${MANPREFIX}/man1/
	chmod 644 ${DESTDIR}${MANPREFIX}/man1/sxbar.1

	mkdir -p ${DESTDIR}${PREFIX}/share
	cp -f default_sxbarc ${DESTDIR}${PREFIX}/share/sxbarc

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/${BIN} \
	      ${DESTDIR}${MANPREFIX}/man1/sxbar.1 \
	      ${DESTDIR}${PREFIX}/share/sxbarc

clangd:
	rm -f compile_flags.txt
	for f in ${CFLAGS}; do echo $$f >> compile_flags.txt; done

.PHONY: all clean install uninstall clangd
