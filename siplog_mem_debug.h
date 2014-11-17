/*
 * Copyright (c) 2014 Sippy Software, Inc., http://www.sippysoft.com
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <string.h>
#include <stdlib.h>

#define malloc(n) siplog_memdeb_malloc((n), __FILE__, __LINE__, __func__)
#define free(p) siplog_memdeb_free((p), __FILE__, __LINE__, __func__)
#define realloc(p,n) siplog_memdeb_realloc((p), (n), __FILE__, __LINE__, __func__)
#define strdup(p) siplog_memdeb_strdup((p), __FILE__, __LINE__, __func__)

void *siplog_memdeb_malloc(size_t, const char *, int, const char *);
void siplog_memdeb_free(void *, const char *, int, const char *);
void *siplog_memdeb_realloc(void *, size_t,  const char *, int, const char *);
char *siplog_memdeb_strdup(const char *, const char *, int, const char *);

#ifndef _SIPLOG_T_DEFINED
typedef void * siplog_t;
#define _SIPLOG_T_DEFINED
#endif

#define SIPLOG_CHECK_LEAKS 	1
