/*
 * Copyright (c) 2004-2006 Maxim Sobolev <sobomax@FreeBSD.org>
 * Copyright (c) 2006-2016 Sippy Software, Inc., http://www.sippysoft.com
 * All rights reserved.
 *
 */

#ifndef _SIPLOG_LOGFILE_ASYNC_H_
#define _SIPLOG_LOGFILE_ASYNC_H_

struct loginfo;

int siplog_logfile_async_open(struct loginfo *);
void siplog_logfile_async_write(struct loginfo *, const char *, const char *,
  const char *, const char *, va_list);
void siplog_logfile_async_close(struct loginfo *);
void siplog_logfile_async_hbeat(struct loginfo *);

#endif
