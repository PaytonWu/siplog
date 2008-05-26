/* $Id$ */

#define _FILE_OFFSET_BITS  64

#include <pthread.h>
#include <sys/file.h>
#include <errno.h>
#include <sched.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "siplog.h"
#include "siplog_internal.h"

#define SIPLOG_WI_POOL_SIZE     64
#define SIPLOG_WI_DATA_LEN      2048

#define SIPLOG_ITEM_ASYNC_OPEN          1
#define SIPLOG_ITEM_ASYNC_WRITE         2
#define SIPLOG_ITEM_ASYNC_CLOSE         3
#define SIPLOG_ITEM_ASYNC_OWRC          4       /* OPEN, WRITE, CLOSE */

#define SIPLOG_WI_NOWAIT	0
#define	SIPLOG_WI_WAIT		1

struct siplog_wi
{
    int item_type;
    struct loginfo *loginfo;
    char data[SIPLOG_WI_DATA_LEN];
    const char *name;
    int len;
    struct siplog_wi *next;
};

static pthread_mutex_t siplog_init_mutex = PTHREAD_MUTEX_INITIALIZER;
static int siplog_queue_inited = 0;
static pthread_t siplog_queue;
static pthread_cond_t siplog_queue_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t siplog_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t siplog_wi_free_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t siplog_wi_free_mutex = PTHREAD_MUTEX_INITIALIZER;

static int siplog_dropped_items;

static struct siplog_wi siplog_wi_pool[SIPLOG_WI_POOL_SIZE];
static struct siplog_wi *siplog_wi_free;
static struct siplog_wi *siplog_wi_queue, *siplog_wi_queue_tail;

static int siplog_queue_init(void);
void siplog_queue_run(void);
struct siplog_wi *siplog_queue_get_free_item(int);
static void siplog_queue_put_item(struct siplog_wi *);
static void siplog_queue_handle_open(struct siplog_wi *);
static void siplog_queue_handle_write(struct siplog_wi *);
static void siplog_queue_handle_close(FILE *f);
static void siplog_queue_handle_owrc(struct siplog_wi *);

#if 0
static void siplog_log_dropped_items(struct siplog_wi *);

static void
siplog_log_dropped_items(struct siplog_wi *wi)
{
    time_t t;
    char *p;

    t = time(&t);
    siplog_timeToStr(t, wi->data);
    p = wi->data + strlen(wi->data);
    sprintf(p, "/GLOBAL/libsiplog: %d message(s) were dropped\n", siplog_dropped_items);
    wi->len = strlen(wi->data);

    switch(wi->item_type) {
	case SIPLOG_ITEM_ASYNC_WRITE:
	    siplog_queue_handle_write(wi);
	    break;
	case SIPLOG_ITEM_ASYNC_OWRC:
	    siplog_queue_handle_owrc(wi);
	    break;
	default:
	    break;
    }

    siplog_dropped_items = 0;
}
#endif

static void
siplog_logfile_async_atexit(void)
{
    struct siplog_wi *wi;
    int c;

    for (;;) {
        pthread_mutex_lock(&siplog_wi_free_mutex);
        for (c = 0, wi = siplog_wi_free; wi != NULL; wi = wi->next) {
            c += 1;
        }
        if (c == SIPLOG_WI_POOL_SIZE)
            break;
        pthread_mutex_unlock(&siplog_wi_free_mutex);
        sched_yield();
    }
    pthread_mutex_unlock(&siplog_wi_free_mutex);
}

static void
siplog_queue_handle_open(struct siplog_wi *wi)
{

    wi->loginfo->private = (void *)fopen(wi->name, "a");
}

static void
siplog_queue_handle_write(struct siplog_wi *wi)
{
    FILE *f;

    f = (FILE *)wi->loginfo->private;
    if (f != NULL) {
	flock(fileno(f), LOCK_EX);
	fwrite(wi->data, wi->len, 1, f);
	fflush(f);
	flock(fileno(f), LOCK_UN);
    }
}

static void
siplog_queue_handle_close(FILE *f)
{

    if (f != NULL)
	fclose(f);
}

static void
siplog_queue_handle_owrc(struct siplog_wi *wi)
{

    siplog_queue_handle_open(wi);
    siplog_queue_handle_write(wi);
    siplog_queue_handle_close((FILE *)wi->loginfo->private);
}

struct siplog_wi *
siplog_queue_get_free_item(int wait)
{
    struct siplog_wi *wi;

    pthread_mutex_lock(&siplog_wi_free_mutex);
    while (siplog_wi_free == NULL) {
	/* no free work items, return if no wait is requested */
	if (wait == 0) {
	    siplog_dropped_items++;
	    pthread_mutex_unlock(&siplog_wi_free_mutex);
	    return NULL;
	}
	pthread_cond_wait(&siplog_wi_free_cond, &siplog_wi_free_mutex);	 
    }

    wi = siplog_wi_free;

    /* move up siplog_wi_free */
    siplog_wi_free = siplog_wi_free->next;
    pthread_mutex_unlock(&siplog_wi_free_mutex);

    return wi;
}

static void
siplog_queue_put_item(struct siplog_wi *wi)
{

    pthread_mutex_lock(&siplog_queue_mutex);

    wi->next = NULL;
    if (siplog_wi_queue == NULL) {
	siplog_wi_queue = wi;
	siplog_wi_queue_tail = wi;
    } else {
	siplog_wi_queue_tail->next = wi;
	siplog_wi_queue_tail = wi;
    }

    /* notify worker thread */
    pthread_cond_signal(&siplog_queue_cond);

    pthread_mutex_unlock(&siplog_queue_mutex);
}

void
siplog_queue_run(void)
{
    struct siplog_wi *wi;

    for (;;) {
	pthread_mutex_lock(&siplog_queue_mutex);
	while (siplog_wi_queue == NULL) {
	    pthread_cond_wait(&siplog_queue_cond, &siplog_queue_mutex);
	}
	wi = siplog_wi_queue;
	siplog_wi_queue = wi->next;
        pthread_mutex_unlock(&siplog_queue_mutex);

        /* main work here */
	switch (wi->item_type) {
	    case SIPLOG_ITEM_ASYNC_OPEN:
		siplog_queue_handle_open(wi);
		break;
	    case SIPLOG_ITEM_ASYNC_WRITE:
		siplog_queue_handle_write(wi);
		break;
	    case SIPLOG_ITEM_ASYNC_CLOSE:
		siplog_queue_handle_close((FILE *)wi->loginfo->private);
		/* free loginfo structure */
		siplog_free(wi->loginfo);
		break;
	    case SIPLOG_ITEM_ASYNC_OWRC:
		siplog_queue_handle_owrc(wi);
		break;
	    default:
		break;
	}

	/* put wi into siplog_wi_free' tail */
	pthread_mutex_lock(&siplog_wi_free_mutex);

#if 0
	/* log dropped items count */
	if (siplog_dropped_items > 0 &&
	    (wi->item_type == SIPLOG_ITEM_ASYNC_WRITE || wi->item_type == SIPLOG_ITEM_ASYNC_OWRC)) {
		pthread_mutex_unlock(&siplog_wi_free_mutex);
		siplog_log_dropped_items(wi);
		pthread_mutex_lock(&siplog_wi_free_mutex);
	}
#endif

	wi->next = siplog_wi_free;
	siplog_wi_free = wi;

	pthread_cond_signal(&siplog_wi_free_cond);
	pthread_mutex_unlock(&siplog_wi_free_mutex);
    }
}

static int
siplog_queue_init(void)
{
    int i;

    memset(siplog_wi_pool, 0, sizeof(siplog_wi_pool));
    for (i = 0; i < SIPLOG_WI_POOL_SIZE - 1; i++) {
	siplog_wi_pool[i].next = &siplog_wi_pool[i + 1];
    }
    siplog_wi_pool[SIPLOG_WI_POOL_SIZE - 1].next = NULL;

    siplog_wi_free = siplog_wi_pool;
    siplog_wi_queue = NULL;
    siplog_wi_queue_tail = NULL;

    siplog_dropped_items = 0;

    if (pthread_create(&siplog_queue, NULL, (void *(*)(void *))&siplog_queue_run, NULL) != 0)
	return -1;

    atexit(siplog_logfile_async_atexit);

    return 0;
}

int
siplog_logfile_async_open(struct loginfo *lp)
{
    struct siplog_wi *wi;

    pthread_mutex_lock(&siplog_init_mutex);
    if (siplog_queue_inited == 0)
	if (siplog_queue_init() != 0)
	    return -1;
    siplog_queue_inited = 1;
    pthread_mutex_unlock(&siplog_init_mutex);

    if ((lp->flags & LF_REOPEN) == 0) {
	wi = siplog_queue_get_free_item(SIPLOG_WI_NOWAIT);
	if (wi == NULL) 
	    return -1;

	wi->item_type = SIPLOG_ITEM_ASYNC_OPEN;
	wi->loginfo = lp;
	wi->len = 0;
	wi->name = getenv("SIPLOG_LOGFILE_FILE");
	if (wi->name == NULL)
            wi->name = SIPLOG_DEFAULT_PATH;

	siplog_queue_put_item(wi);
    }
    return 0;
}

void
siplog_logfile_async_write(struct loginfo *lp, const char *tstamp, const char *estr,
  const char *fmt, va_list ap)
{
    struct siplog_wi *wi;
    char *p;
    int s1, s2;

    wi = siplog_queue_get_free_item(SIPLOG_WI_NOWAIT);
    if (wi == NULL)
	return;

    do {
	p = wi->data;
	s1 = sizeof(wi->data);

	s2 = snprintf(p, s1, "%s/%s/%s: ", tstamp, lp->call_id, lp->app);
	if (s2 >= s1)	/* message was truncated */
	    break;
	p += s2;
	s1 -= s2;

	s2 = vsnprintf(p, s1, fmt, ap);
	if (s2 >= s1)	/* message was truncated */
	    break;
	p += s2;
	s1 -= s2;

	if (estr != NULL) {
	    s2 = snprintf(p, s1, fmt, ap);
	    if (s2 >= s1)	/* message was truncated */
		break;
	    p += s2;
	    s1 -= s2;
	}

	snprintf(p, s1, "\n");

    } while (0);


    if ((lp->flags & LF_REOPEN) != 0) {
	wi->item_type = SIPLOG_ITEM_ASYNC_OWRC;
	wi->name = getenv("SIPLOG_LOGFILE_FILE");
	if (wi->name == NULL)
	    wi->name = SIPLOG_DEFAULT_PATH;
    } else {
	wi->item_type = SIPLOG_ITEM_ASYNC_WRITE;
    }
    wi->loginfo = lp;
    wi->len = strlen(wi->data);

    siplog_queue_put_item(wi);
}

void
siplog_logfile_async_close(struct loginfo *lp)
{
    struct siplog_wi *wi;

    if ((lp->flags & LF_REOPEN) == 0) {
	wi = siplog_queue_get_free_item(SIPLOG_WI_WAIT);
	wi->item_type = SIPLOG_ITEM_ASYNC_CLOSE;
	wi->loginfo = lp;
	wi->len = 0;

	siplog_queue_put_item(wi);
    }
}
