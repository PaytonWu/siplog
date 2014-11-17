# $Id$

PKGNAME=	${LIB}
PKGFILES=	GNUmakefile Makefile ${SRCS} ${DEBUG_SRCS} test.c

LIB=		siplog
LIBTHREAD?=	pthread
.if defined(SIPLOG_DEBUG)
CFLAGS+=	-DSIPLOG_DEBUG -include siplog_mem_debug.h
SRCS+=		${DEBUG_SRCS}
.endif
DEBUG_SRCS=	siplog_mem_debug.c siplog_mem_debug.h

SRCS+=		siplog.c siplog.h siplog_internal.h siplog_logfile_async.c

LDADD=		-l${LIBTHREAD}

NO_SHARED?=	YES
NO_PROFILE?=	YES

WARNS?=		4

CLEANFILES+=	test

test: lib${LIB}.a test.c
	${CC} ${CFLAGS} -I. test.c -o test -l${LIBTHREAD} -L. -l${LIB}

TSTAMP!=        date "+%Y%m%d%H%M%S"

distribution: clean
	tar cvfy /tmp/${PKGNAME}-sippy-${TSTAMP}.tbz2 ${PKGFILES}
	scp /tmp/${PKGNAME}-sippy-${TSTAMP}.tbz2 sobomax@download.sippysoft.com:/usr/local/www/data/siplog/
	git tag rel.${TSTAMP}
	git push origin rel.${TSTAMP}

includepolice:
	for file in ${SRCS}; do \
	  python misc/includepolice.py $${file} || sleep 5; \
	done

.include <bsd.lib.mk>
