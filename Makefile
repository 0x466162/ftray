.include <bsd.xconf.mk>
CC	?= gcc
STRIP ?= strip
CFLAGS += -std=c99 -fshort-wchar -Os -I${X11BASE}/include -I${X11BASE}/include/freetype2
LDFLAGS += -L${X11BASE}/lib -lX11 -lX11-xcb -lxcb-aux -lxcb-icccm -lxcb-keysyms -lxcb-randr -lxcb-xtest -lXft -lXcursor
CFDEBUG = -g3 -pedantic -Wall -Wunused-parameter -Wlong-long\
		  -Wsign-conversion -Wconversion -Wimplicit-function-declaration

EXEC = ftray
SRCS = ftray.c
OBJS = ${SRCS:.c=.o}

PREFIX?=/usr/local
BINDIR=${PREFIX}/bin

all: ${EXEC}

.c.o:
	${CC} ${CFLAGS} -o $@ -c $<

#${OBJS}: config.h
#
#config.h:
#	@echo creating $@ from config.def.h
#	@cp config.def.h $@

${EXEC}:
	${CC} ${LDFLAGS} -o ${EXEC} ${OBJS}
	${STRIP} -s ${EXEC}

debug: ${EXEC}
debug: CC += ${CFDEBUG}

clean:
	rm -rf ./obj/*.o
	rm -rf ./${EXEC}

install: foo
	test -d ${DESTDIR}${BINDIR} || mkdir -p ${DESTDIR}${BINDIR}
	install -m755 foo ${DESTDIR}${BINDIR}/foo

uninstall:
	rm -f ${DESTDIR}${BINDIR}/foo

.PHONY: all debug clean install

.include <bsd.prog.mk>
.include <bsd.xorg.mk>
