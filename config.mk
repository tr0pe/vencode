VERSION = 1.5.1

# Customize below to fit your system
PREFIX  = /usr/local
INCS    = -I/usr/include/libpng16
LIBS    = -lpng16
CFLAGS  = -O2 -march=native -std=c99 -Wall ${INCS} -DVERSION=\"${VERSION}\"
LDFLAGS = ${LIBS}
CC      = cc
