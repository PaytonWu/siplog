/* $Id$ */

#include <err.h>
#include <siplog.h>
#include <stdlib.h>

int main()
{
    siplog_t log, globallog;

    log = siplog_open("test", "1234@1.2.3.4");
    globallog = siplog_open("test", NULL);
    if (log == NULL)
        err(1, "can't open logs");
    siplog_write(SIPLOG_DBUG, globallog, "staring process...");
    siplog_write(SIPLOG_DBUG, log, "level DBUG, %d", 1);
    siplog_write(SIPLOG_WARN, log, "level WARN, %d", 2);
    siplog_write(SIPLOG_ERR, log, "level ERR, %d", 3);
    siplog_write(SIPLOG_DBUG, globallog, "stoping process...");
    siplog_close(globallog);
    siplog_close(log);

    exit(0);
}
