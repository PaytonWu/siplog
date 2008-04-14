/* $Id$ */

#include <err.h>
#include <siplog.h>
#include <stdlib.h>
#include <time.h>

int main()
{
    siplog_t log, globallog;
    int i;
    struct timespec interval;

    log = siplog_open("test", "1234@1.2.3.4", 0);
    globallog = siplog_open("test", NULL, LF_REOPEN);
    if (log == NULL)
        err(1, "can't open logs");
    siplog_write(SIPLOG_DBUG, globallog, "staring process...");
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
    siplog_close(globallog);
    siplog_close(log);

    /* allow worker thread to finish its job in async mode */
    sleep(1);

    exit(0);
}
