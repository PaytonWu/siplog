/*
 * Copyright (c) 2004-2006 Maxim Sobolev <sobomax@FreeBSD.org>
 * Copyright (c) 2006-2016 Sippy Software, Inc., http://www.sippysoft.com
 * All rights reserved.
 *
 */

#include <err.h>
#include <siplog.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

int main()
{
    siplog_t log, globallog;
    int i;
    struct timespec interval;

    globallog = siplog_open("test", NULL, LF_REOPEN);
    siplog_memdeb_setbaseln();
    siplog_write(SIPLOG_DBUG, globallog, "staring process...");
    log = siplog_open("test", "1234@1.2.3.4", 0);
    if (log == NULL || globallog == NULL)
        err(1, "can't open logs");
    siplog_write(SIPLOG_DBUG, log, "level DBUG, %d", 1);
    siplog_write(SIPLOG_WARN, log, "level WARN, %d", 2);
    siplog_write(SIPLOG_ERR, log, "level ERR, %d", 3);
    for (i = 0; i < 10000;) {
	siplog_write(SIPLOG_DBUG, log, "message #%d", ++i);
	/* sleep 0.0000001 second */
	interval.tv_sec = 0; 
	interval.tv_nsec = 100;
	nanosleep(&interval, NULL);
    }
    siplog_write(SIPLOG_DBUG, globallog, "stoping process...");
    siplog_close(log);
    siplog_memdeb_dumpstats(SIPLOG_DBUG, globallog);
    siplog_close(globallog);

    /* allow worker thread to finish its job in async mode */
    sleep(1);

    exit(0);
}
