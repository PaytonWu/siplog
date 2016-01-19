/*
 * Copyright (c) 2004-2006 Maxim Sobolev <sobomax@FreeBSD.org>
 * Copyright (c) 2006-2016 Sippy Software, Inc., http://www.sippysoft.com
 * All rights reserved.
 *
 */

#ifndef _SIPLOG_INTERNAL_H_
#define _SIPLOG_INTERNAL_H_

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
    pid_t       pid;
};

typedef int    (*siplog_bend_open_t)(struct loginfo *);
typedef void   (*siplog_bend_write_t)(struct loginfo *, const char *, const char *,
				      const char *, const char *, va_list);
typedef void   (*siplog_bend_close_t)(struct loginfo *);
typedef void   (*siplog_bend_hbeat_t)(struct loginfo *);

struct bend
{
    siplog_bend_open_t  open;
    siplog_bend_write_t write;
    siplog_bend_close_t close;
    siplog_bend_hbeat_t hbeat;
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
void siplog_update_index(const char *, int, off_t, size_t);

#endif /* _SIPLOG_INTERNAL_H_ */
