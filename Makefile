# $Id$

LIB=		siplog
LIBTHREAD?=	pthread

SRCS=		siplog.c siplog.h siplog_internal.h siplog_logfile_async.c

LDADD=		-l{LIBTHREAD}

NO_SHARED?=	YES
NO_PROFILE?=	YES

WARNS?=		4

CLEANFILES+=	test

test: lib${LIB}.a
	${CC} -I. test.c -o test -l${LIBTHREAD} -L. -l${LIB}

.include <bsd.lib.mk>
