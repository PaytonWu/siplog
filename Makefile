# $Id$

PKGNAME=	${LIB}
PKGFILES=	GNUmakefile Makefile ${SRCS} test.c

LIB=		siplog
LIBTHREAD?=	pthread

SRCS=		siplog.c siplog.h siplog_internal.h siplog_logfile_async.c

LDADD=		-l${LIBTHREAD}

NO_SHARED?=	YES
NO_PROFILE?=	YES

WARNS?=		4

CLEANFILES+=	test

test: lib${LIB}.a
	${CC} -I. test.c -o test -l${LIBTHREAD} -L. -l${LIB}

TSTAMP!=        date "+%Y%m%d%H%M%S"

distribution: clean
	tar cvfy /tmp/${PKGNAME}-sippy-${TSTAMP}.tbz2 ${PKGFILES}
	scp /tmp/${PKGNAME}-sippy-${TSTAMP}.tbz2 sobomax@download.sippysoft.com:/usr/local/www/data/siplog/
	git tag rel.${TSTAMP}
	git push origin rel.${TSTAMP}

.include <bsd.lib.mk>
