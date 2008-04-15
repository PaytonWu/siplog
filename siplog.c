/* $Id$ */

#define _FILE_OFFSET_BITS  64

#include <sys/file.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "siplog.h"
#include "siplog_internal.h"

#define assert(x) {if (!(x)) abort();}

static struct
{
    const char *descr;
    int level;
} levels[] = {
    {"DBUG", SIPLOG_DBUG},
    {"INFO", SIPLOG_INFO},
    {"WARN", SIPLOG_WARN},
    {"ERR",  SIPLOG_ERR},
    {"CRIT", SIPLOG_CRIT},
    {NULL,   0}
};

static void * siplog_stderr_open(struct loginfo *);
static void   siplog_stderr_write(struct loginfo *, const char *, const char *,
				  const char *, va_list);
static void   siplog_stderr_close(struct loginfo *);
static void * siplog_logfile_open(struct loginfo *);
static void   siplog_logfile_write(struct loginfo *, const char *, const char *,
		    		   const char *, va_list);
static void   siplog_logfile_close(struct loginfo *);

static struct bend bends[] = {
    {siplog_stderr_open, siplog_stderr_write, siplog_stderr_close, 1, "stderr"},
    {siplog_logfile_open, siplog_logfile_write, siplog_logfile_close, 1, "logfile"},
    {siplog_logfile_async_open, siplog_logfile_async_write, siplog_logfile_async_close, 0, "logfile_async"},
    {NULL, NULL, NULL, 0, NULL}
};

char *
siplog_timeToStr(time_t uTime, char *buf)
{
    static const char *wdays[7] = {"Sun", "Mon", "Tue", "Wed", "Thu",
      "Fri", "Sat"};
    static const char *mons[12] = {"Jan", "Feb", "Mar", "Apr", "May",
      "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    const char *wday, *mon;
    struct tm mytm;

    localtime_r(&uTime, &mytm);
    assert(mytm.tm_wday < 7);
    assert(mytm.tm_mon < 12);
    mon = mons[mytm.tm_mon];
    wday = wdays[mytm.tm_wday];
#ifdef SIPLOG_DETAILED_DATES
    sprintf(buf, "%.2d:%.2d:%.2d %s %s %s %.2d %d", mytm.tm_hour,
      mytm.tm_min, mytm.tm_sec, mytm.tm_zone, wday, mon, mytm.tm_mday,
      mytm.tm_year + 1900);
#else
    sprintf(buf, "%d %s %.2d:%.2d:%.2d", mytm.tm_mday,
      mon, mytm.tm_hour, mytm.tm_min, mytm.tm_sec);
#endif
    return (buf);
}

static void *
siplog_stderr_open(struct loginfo *lp __attribute__ ((unused)))
{

    return (void *)stderr;
}

static void
siplog_stderr_write(struct loginfo *lp, const char *tstamp, const char *estr, const char *fmt,
		    va_list ap)
{
    FILE *f;

    f = (FILE *)lp->stream;
    fprintf(f, "%s/%s/%s: ", tstamp, lp->call_id, lp->app);
    vfprintf(f, fmt, ap);
    if (estr != NULL)
	fprintf(f, ": %s", estr);
    fprintf(f, "\n");
}

static void
siplog_stderr_close(struct loginfo *lp __attribute__ ((unused)))
{

    /* Nothing to do here */
}

static void *
siplog_logfile_open(struct loginfo *lp)
{
    const char *cp;

    cp = getenv("SIPLOG_LOGFILE_FILE");
    if (cp == NULL)
	cp = SIPLOG_DEFAULT_PATH;

    if ((lp->flags & LF_REOPEN) == 0)
        return (void *)fopen(cp, "a");
    else
	return (void *)0x1;
}

static void
siplog_logfile_write(struct loginfo *lp, const char *tstamp, const char *estr, const char *fmt,
		     va_list ap)
{
    FILE *f;

    if ((lp->flags & LF_REOPEN) == 0) {
	f = (FILE *)lp->stream;
    } else {
	const char *cp;

	cp = getenv("SIPLOG_LOGFILE_FILE");
	if (cp == NULL)
	    cp = SIPLOG_DEFAULT_PATH;

	f = fopen(cp, "a");
	if (f == NULL)
	    return;
    }
    flock(fileno(f), LOCK_EX);
    fprintf(f, "%s/%s/%s: ", tstamp, lp->call_id, lp->app);
    vfprintf(f, fmt, ap);
    if (estr != NULL)
	fprintf(f, ": %s", estr);
    fprintf(f, "\n");
    fflush(f);
    flock(fileno(f), LOCK_UN);
    if ((lp->flags & LF_REOPEN) != 0)
	fclose(f);
}

static void
siplog_logfile_close(struct loginfo *lp)
{

    if ((lp->flags & LF_REOPEN) == 0)
        fclose((FILE *)lp->stream);
}

siplog_t
siplog_open(const char *app, const char *call_id, int flags)
{
    int i;
    struct loginfo *lp;
    const char *el, *sb;

    lp = malloc(sizeof(*lp));
    if (lp == NULL)
        return NULL;
    memset(lp, '\0', sizeof(*lp));

    lp->app = strdup(app);
    if (lp->app == NULL) {
        free(lp);
        return NULL;
    }

    lp->call_id = (call_id != NULL) ? strdup(call_id) : strdup("GLOBAL");
    if (lp->call_id == NULL) {
	free(lp->app);
	free(lp);
	return NULL;
    }

    lp->bend = &(bends[0]);
    sb = getenv("SIPLOG_BEND");
    for (i = 0; sb != NULL && bends[i].name != NULL; i++) {
        if (strcmp(sb, bends[i].name) == 0) {
            lp->bend = &(bends[i]);
            break;
        }
    }

    lp->level = SIPLOG_DBUG;
    lp->flags = flags;
    el = getenv("SIPLOG_LVL");
    for (i = 0; el != NULL && levels[i].descr != NULL; i++) {
        if (strcmp(el, levels[i].descr) == 0) {
            lp->level = levels[i].level;
            break;
        }
    }

    lp->stream = lp->bend->open(lp);
    if (lp->stream == NULL) {
        free(lp->call_id);
        free(lp->app);
        free(lp);
        return NULL;
    }

    return lp;
}

void
siplog_write(int level, siplog_t handle, const char *fmt, ...)
{
    struct loginfo *lp;
    char tstamp[64];
    time_t t;
    va_list ap;

    lp = (struct loginfo *)handle;
    if (lp == NULL || lp->bend == NULL || level < lp->level)
        return;
    t = time(&t);
    siplog_timeToStr(t, tstamp);
    va_start(ap, fmt);
    lp->bend->write(lp, tstamp, NULL, fmt, ap);
    va_end(ap);
}

void
siplog_ewrite(int level, siplog_t handle, const char *fmt, ...)
{
    struct loginfo *lp;
    char tstamp[64];
    char ebuf[256];
    time_t t;
    va_list ap;
    int errno_bak;

    lp = (struct loginfo *)handle;
    if (lp == NULL || level < lp->level)
        return;
    errno_bak = errno;
    if (strerror_r(errno, ebuf, sizeof(ebuf)) != 0) {
	errno = errno_bak;
	return;
    }
    t = time(&t);
    siplog_timeToStr(t, tstamp);
    va_start(ap, fmt);
    lp->bend->write(lp, tstamp, ebuf, fmt, ap);
    va_end(ap);
    errno = errno_bak;
}

void
siplog_close(siplog_t handle)
{
    struct loginfo *lp;

    lp = (struct loginfo *)handle;
    if (lp == NULL)
        return;
    lp->bend->close(lp);
    if (lp->bend->free_after_close) {
	siplog_free(lp);
    }
}

void
siplog_free(struct loginfo *lp)
{

    free(lp->call_id);
    free(lp->app);
    free(lp);
}
