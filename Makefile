# See LICENSE file for copyright and license details.

include config.mk

SRC = vencode.c util.c args.c
OBJ = ${SRC:.c=.o}

all: vencode

.c.o:
	${CC} -c ${CFLAGS} $<

vencode: ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS}

clean:
	rm -f vencode ${OBJ}

install: all
	mkdir -p ${PREFIX}/bin
	cp -f vencode ${PREFIX}/bin
	chmod 755 ${PREFIX}/bin/vencode

uninstall:
	rm -f ${PREFIX}/bin/vencode

.PHONY: all clean install uninstall
