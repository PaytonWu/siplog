CC?=	gcc
AR?=	ar
CFLAGS=-Wall -pedantic

LIB=		siplog
LIBTHREAD?=	pthread

all: lib${LIB}.a

lib${LIB}.a: siplog.o siplog_logfile_async.o
	${AR} cru lib${LIB}.a siplog.o siplog_logfile_async.o

siplog.o: siplog.c siplog.h siplog_internal.h
	${CC} ${CFLAGS} -o siplog.o -c siplog.c

siplog_logfile_async.o: siplog_logfile_async.c siplog.h siplog_internal.h
	${CC} ${CFLAGS} -o siplog_logfile_async.o -c siplog_logfile_async.c

test: lib${LIB}.a
	${CC} -I. test.c -o test -l${LIBTHREAD} -L. -l${LIB}

clean:
	rm -f lib${LIB}.a siplog.o siplog_logfile_async.o test
