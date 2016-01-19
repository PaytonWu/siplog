/*
 * Copyright (c) 2004-2006 Maxim Sobolev <sobomax@FreeBSD.org>
 * Copyright (c) 2006-2016 Sippy Software, Inc., http://www.sippysoft.com
 * All rights reserved.
 *
 */

#define _FILE_OFFSET_BITS  64

#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "siplog.h"
#include "internal/_siplog.h"
#include "internal/siplog_logfile_async.h"

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

static int    siplog_stderr_open(struct loginfo *);
static void   siplog_stderr_write(struct loginfo *, const char *, const char *,
				  const char *, const char *, va_list);
static void   siplog_stderr_close(struct loginfo *);
static int    siplog_logfile_open(struct loginfo *);
static void   siplog_logfile_write(struct loginfo *, const char *, const char *,
		    		   const char *, const char *, va_list);
static void   siplog_logfile_close(struct loginfo *);

static struct bend bends[] = {
    {.open = siplog_stderr_open, .write = siplog_stderr_write,
      .close = siplog_stderr_close, .free_after_close = 1, .name = "stderr"},
    {.open = siplog_logfile_open, .write = siplog_logfile_write,
      .close = siplog_logfile_close, .free_after_close = 1, .name = "logfile"},
    {.open = siplog_logfile_async_open, .write = siplog_logfile_async_write,
      .close = siplog_logfile_async_close, .free_after_close = 0,
      .name = "logfile_async", .hbeat = siplog_logfile_async_hbeat},
    {.open = NULL, .write = NULL, .close = NULL, .name = NULL}
};

char *
siplog_timeToStr(struct timeval *tvp, char *buf)
{
    static const char *wdays[7] = {"Sun", "Mon", "Tue", "Wed", "Thu",
      "Fri", "Sat"};
    static const char *mons[12] = {"Jan", "Feb", "Mar", "Apr", "May",
      "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    const char *wday, *mon;
    struct tm mytm;

    localtime_r((time_t *)&(tvp->tv_sec), &mytm);
    assert(mytm.tm_wday < 7);
    assert(mytm.tm_mon < 12);
    mon = mons[mytm.tm_mon];
    wday = wdays[mytm.tm_wday];
#ifdef SIPLOG_DETAILED_DATES
    sprintf(buf, "%.2d:%.2d:%.2d %s %s %s %.2d %d", mytm.tm_hour,
      mytm.tm_min, mytm.tm_sec, mytm.tm_zone, wday, mon, mytm.tm_mday,
      mytm.tm_year + 1900);
#else
    sprintf(buf, "%d %s %.2d:%.2d:%.2d.%.3d", mytm.tm_mday,
      mon, mytm.tm_hour, mytm.tm_min, mytm.tm_sec, (int)(tvp->tv_usec / 1000));
#endif
    return (buf);
}

static int
siplog_stderr_open(struct loginfo *lp)
{

    lp->private = (void *)stderr;
    return (lp->private != NULL ? 0 : -1);
}

static void
siplog_stderr_write(struct loginfo *lp, const char *tstamp,
  const char *estr, const char *unused __attribute__ ((unused)),
  const char *fmt, va_list ap)
{
    FILE *f;

    f = (FILE *)lp->private;
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

void
siplog_update_index(const char *idx_id, int fd, off_t offset, size_t nbytes)
{
    struct stat st;
    int res, idxfile;
    char *fname, *outbuf;

    res = fstat(fd, &st);
    if (res == -1)
        return;
    asprintf(&fname, "/var/log/siplog.idx/%llu", (long long unsigned)st.st_ino);
    if (fname == NULL)
        return;
    idxfile = open(fname, O_CREAT | O_APPEND | O_WRONLY, 0644);
    free(fname);
    if (idxfile < 0)
        return;
    res = asprintf(&outbuf, "%s %llu %llu\n", idx_id,
      (long long unsigned)offset, (long long unsigned)nbytes);
    if (outbuf == NULL) {
        close(idxfile);
        return;
    }
    write(idxfile, outbuf, res);
    close(idxfile);
    free(outbuf);
}

static int
siplog_logfile_open(struct loginfo *lp)
{
    const char *cp;

    cp = getenv("SIPLOG_LOGFILE_FILE");
    if (cp == NULL)
	cp = SIPLOG_DEFAULT_PATH;

    if ((lp->flags & LF_REOPEN) == 0) {
        lp->private = (void *)fopen(cp, "a");
        if (lp->private == NULL)
            return -1;
    }
    return 0;
}

static void
siplog_logfile_write(struct loginfo *lp, const char *tstamp,
  const char *estr, const char *idx_id, const char *fmt, va_list ap)
{
    FILE *f;
    off_t offset;
    size_t nbytes;

    if ((lp->flags & LF_REOPEN) == 0) {
	f = (FILE *)lp->private;
    } else {
	const char *cp;

	cp = getenv("SIPLOG_LOGFILE_FILE");
	if (cp == NULL)
	    cp = SIPLOG_DEFAULT_PATH;

	f = fopen(cp, "a");
	if (f == NULL)
	    return;
    }
    offset = siplog_lockf(fileno(f));
    nbytes = fprintf(f, "%s/%s/%s: ", tstamp, lp->call_id, lp->app);
    nbytes += vfprintf(f, fmt, ap);
    if (estr != NULL)
	nbytes += fprintf(f, ": %s", estr);
    nbytes += fprintf(f, "\n");
    fflush(f);
    siplog_unlockf(fileno(f), offset);
    siplog_update_index(idx_id, fileno(f), offset, nbytes);
    if ((lp->flags & LF_REOPEN) != 0)
	fclose(f);
}

static void
siplog_logfile_close(struct loginfo *lp)
{

    if ((lp->flags & LF_REOPEN) == 0)
        fclose((FILE *)lp->private);
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

    if (call_id != NULL) {
        lp->call_id = strdup(call_id);
        lp->call_id_global = 0;
    } else {
        lp->call_id = strdup("GLOBAL");
        lp->call_id_global = 1;
    }
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

    /* Detect uninitialized access */
    lp->private = (void *)0x1;
    lp->pid = getpid();

    if (lp->bend->open(lp) != 0) {
        siplog_free(lp);
        return NULL;
    }

    return lp;
}

int
siplog_set_level(siplog_t handle, int level)
{
    struct loginfo *lp;
    int oldlevel;

    lp = (struct loginfo *)handle;
    if (lp == NULL || lp->bend == NULL)
        return -1;

    if (level < 0)
        return lp->level;
    oldlevel = lp->level;
    lp->level = level;

    return oldlevel;
} 

void
siplog_write_va(int level, siplog_t handle, const char *fmt, va_list ap)
{
    struct loginfo *lp;
    char tstamp[64];
    struct timeval tv;
    const char *idx_id;

    lp = (struct loginfo *)handle;
    if (lp == NULL || lp->bend == NULL || level < lp->level)
        return;
    gettimeofday(&tv, NULL);
    siplog_timeToStr(&tv, tstamp);
    idx_id = (lp->call_id_global != 0) ? NULL : lp->call_id;
    lp->bend->write(lp, tstamp, NULL, idx_id, fmt, ap);
}

void
siplog_write(int level, siplog_t handle, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    siplog_write_va(level, handle, fmt, ap);
    va_end(ap);
}

void
siplog_iwrite(int level, siplog_t handle, const char *idx_id, const char *fmt, ...)
{
    struct loginfo *lp;
    char tstamp[64];
    struct timeval tv;
    va_list ap;

    lp = (struct loginfo *)handle;
    if (lp == NULL || lp->bend == NULL || level < lp->level)
        return;
    gettimeofday(&tv, NULL);
    siplog_timeToStr(&tv, tstamp);
    va_start(ap, fmt);
    lp->bend->write(lp, tstamp, NULL, idx_id, fmt, ap);
    va_end(ap);
}

void
siplog_ewrite_va(int level, siplog_t handle, const char *fmt, va_list ap)
{
    struct loginfo *lp;
    char tstamp[64];
    char ebuf[256];
    struct timeval tv;
    int errno_bak;
    const char *idx_id;

    lp = (struct loginfo *)handle;
    if (lp == NULL || level < lp->level)
        return;
    errno_bak = errno;
    if (strerror_r(errno, ebuf, sizeof(ebuf)) != 0) {
	errno = errno_bak;
	return;
    }
    gettimeofday(&tv, NULL);
    siplog_timeToStr(&tv, tstamp);
    idx_id = (lp->call_id_global != 0) ? NULL : lp->call_id;
    lp->bend->write(lp, tstamp, ebuf, idx_id, fmt, ap);
    errno = errno_bak;
}

void
siplog_ewrite(int level, siplog_t handle, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    siplog_ewrite_va(level, handle, fmt, ap);
    va_end(ap);
}

void
siplog_close(siplog_t handle)
{
    struct loginfo *lp;
    int free_after_close;

    lp = (struct loginfo *)handle;
    if (lp == NULL)
        return;
    free_after_close = lp->bend->free_after_close;
    lp->bend->close(lp);
    if (free_after_close) {
	siplog_free(lp);
    }
}

void
siplog_hbeat(siplog_t handle)
{
    struct loginfo *lp;

    lp = (struct loginfo *)handle;
    if (lp == NULL || lp->bend->hbeat == NULL)
        return;
    lp->bend->hbeat(lp);
}

void
siplog_free(struct loginfo *lp)
{

    free(lp->call_id);
    free(lp->app);
    free(lp);
}

off_t
siplog_lockf(int fd)
{
    struct flock l;
    int rval;

    memset(&l, '\0', sizeof(l));
    l.l_whence = SEEK_CUR;
    l.l_type = F_WRLCK;
    do {
        rval = fcntl(fd, F_SETLKW, &l);
    } while (rval == -1 && errno == EINTR);
#if defined(PEDANTIC)
    assert(rval != -1);
#endif
    return lseek(fd, 0, SEEK_CUR);
}

void
siplog_unlockf(int fd, off_t offset)
{
    struct flock l;
    int rval;

    memset(&l, '\0', sizeof(l));
    l.l_whence = SEEK_SET;
    l.l_start = offset;
    l.l_type = F_UNLCK;
    do {
        rval = fcntl(fd, F_SETLKW, &l);
    } while (rval == -1 && errno == EINTR);
#if defined(PEDANTIC)
    assert(rval != -1);
#endif
}
