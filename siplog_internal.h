/* $Id$ */

#ifndef _SIPLOG_INTERNAL_H_
#define _SIPLOG_INTERNAL_H_

#include <sys/time.h>
#include <stdarg.h>
#include <stdio.h>

#define SIPLOG_DEFAULT_PATH	"/var/log/sip.log"

struct loginfo
{
    void        *private;
    char        *app;
    char        *call_id;
    int         level;
    struct bend *bend;
    int         flags;
    int         call_id_global;
};

typedef int    (*siplog_bend_open_t)(struct loginfo *);
typedef void   (*siplog_bend_write_t)(struct loginfo *, const char *, const char *,
				      const char *, const char *, va_list);
typedef void   (*siplog_bend_close_t)(struct loginfo *);

struct bend
{
    siplog_bend_open_t  open;
    siplog_bend_write_t write;
    siplog_bend_close_t close;
    int			free_after_close;
    const char          *name;
};

int siplog_logfile_async_open(struct loginfo *);
void siplog_logfile_async_write(struct loginfo *, const char *, const char *,
  const char *, const char *, va_list);
void siplog_logfile_async_close(struct loginfo *);

char *siplog_timeToStr(struct timeval *, char *);
void siplog_free(struct loginfo *);
off_t siplog_lockf(int);
void siplog_unlockf(int, off_t);
void siplog_update_index(const char *, int, off_t);

#endif /* _SIPLOG_INTERNAL_H_ */
