# $Id$

LIB=		siplog

SRCS=		siplog.c siplog.h siplog_internal.h

NOSHARED?=	YES
NOPROFILE?=	YES

WARNS?=		4

CLEANFILES+=	test

test: lib${LIB}.a
	${CC} -I. test.c -o test lib${LIB}.a

.include <bsd.lib.mk>
