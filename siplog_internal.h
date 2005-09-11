/* $Id$ */

#ifndef _SIPLOG_INTERNAL_H_
#define _SIPLOG_INTERNAL_H_

#include <stdarg.h>
#include <stdio.h>

struct loginfo
{
    void        *stream;
    char        *app;
    char        *call_id;
    int         level;
    struct bend *bend;
    int         flags;
};

typedef void * (*siplog_bend_open_t)(struct loginfo *);
typedef void   (*siplog_bend_write_t)(struct loginfo *, const char *, const char *,
				      const char *, va_list);
typedef void   (*siplog_bend_close_t)(struct loginfo *);

struct bend
{
    siplog_bend_open_t  open;
    siplog_bend_write_t write;
    siplog_bend_close_t close;
    const char          *name;
};

#endif /* _SIPLOG_INTERNAL_H_ */
