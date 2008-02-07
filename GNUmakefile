CC?=	gcc
AR?=	ar

all: libsiplog.a

libsiplog.a: siplog.o
	${AR} cru libsiplog.a siplog.o

siplog.o: siplog.c siplog.h  siplog_internal.h
	${CC} ${CFLAGS} -o siplog.o -c siplog.c

clean:
	rm -f libsiplog.a siplog.o
