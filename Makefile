INCLUDEDIR:=include
LIBS:= -lfcgi -L/home/alexander/fcgi-i/lib -ldl -lgdbm
FCGI_INCLUDES:=-I/home/alexander/fcgi-i/include

DESTDIR:=/var/www/html
DEFINES:=-D_REENTRANT 
CFLAGS:=-g -O0 -Wall -rdynamic -std=gnu89
LDFLAGS=: -liconv 
CC:=gcc
PROG:=wmail
INCLUDES:= -I${INCLUDEDIR} ${FCGI_INCLUDES}
DEPDIR:=deps

SOURCES:=$(wildcard *.c)
OBJECTS:=${SOURCES:.c=.o}

.PHONY: clean install check depend

all: check depend ${PROG}

$(PROG): ${OBJECTS}
	${CC} ${CFLAGS} $^ -o $@ ${LIBS}

${OBJECTS}: ${SOURCES} 
	${CC} ${CFLAGS} ${INCLUDES} ${DEFINES} $^ -c

depend: 
	${CC} ${INCLUDES} -M -MM -MD ${SOURCES}
	[ -d "${DEPDIR}" ] || mkdir ${DEPDIR}
	mv *.d ${DEPDIR}

clean:
	rm -f ${DEPDIR}/*.d
	rm -f *.o ${PROG}

check:
	@if [ "`uname`" != "Linux" ]; then \
		echo "Sorry, linux required, not `uname`"; \
		exit 1; \
	fi

install:
ifneq ($(shell pgrep wmail.fcgi),)
	killall -9 wmail.fcgi
endif
	cp wmail ${DESTDIR}/wmail.fcgi
	cp -r templates ${DESTDIR}
	cp -r images ${DESTDIR}
	cp wm.css script.js Menu.js ${DESTDIR}
#	@mkdir -p ${DESTDIR}/${PREFIX}/sbin
#	install -m 750 ulogd ${DESTDIR}/${PREFIX}/sbin
#	install -m 755 ulogd.init ${DESTDIR}/etc/init.d/ulogd

-include ${DEPDIR}/*.d
