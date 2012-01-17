/* $Id$ */

#define _FILE_OFFSET_BITS  64

#include <sys/file.h>
#include <pthread.h>
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
#define SIPLOG_WI_ID_LEN        128

typedef enum {
    SIPLOG_ITEM_ASYNC_OPEN,
    SIPLOG_ITEM_ASYNC_WRITE,
    SIPLOG_ITEM_ASYNC_CLOSE,
    SIPLOG_ITEM_ASYNC_OWRC, /* OPEN, WRITE, CLOSE */
    SIPLOG_ITEM_ASYNC_EXIT
} item_types;

#define SIPLOG_WI_NOWAIT	0
#define	SIPLOG_WI_WAIT		1

struct siplog_private {
    int fd;
};

struct siplog_wi
{
    item_types item_type;
    struct loginfo *loginfo;
    char data[SIPLOG_WI_DATA_LEN];
    const char *name;
    int len;
    struct siplog_wi *next;
    char idx_id[SIPLOG_WI_ID_LEN];
};

static pthread_mutex_t siplog_init_mutex = PTHREAD_MUTEX_INITIALIZER;
static int siplog_queue_inited = 0;
static int atexit_registered = 0;
static int atfork_registered = 0;
static pthread_t siplog_queue;
static pthread_cond_t siplog_queue_cond;
static pthread_mutex_t siplog_queue_mutex;
static pthread_cond_t siplog_wi_free_cond;
static pthread_mutex_t siplog_wi_free_mutex;

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
static void siplog_queue_handle_close(struct siplog_wi *);
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

    if (siplog_queue_inited == 0)
	return;

    /* Wait for the worker thread to exit */
    wi = siplog_queue_get_free_item(SIPLOG_WI_WAIT);
    wi->item_type = SIPLOG_ITEM_ASYNC_EXIT;
    siplog_queue_put_item(wi);
    pthread_join(siplog_queue, NULL);
    siplog_queue_inited = 0;
}

static void
siplog_logfile_async_atfork(void)
{

    siplog_logfile_async_atexit();
    atfork_registered = 0;
}

static void
siplog_queue_handle_open(struct siplog_wi *wi)
{
    struct siplog_private *private;

    private = (struct siplog_private *)wi->loginfo->private;

    private->fd = open(wi->name, O_CREAT | O_APPEND | O_WRONLY, 0640);
}

static void
siplog_queue_handle_write(struct siplog_wi *wi)
{
    off_t offset;
    struct siplog_private *private;

    private = (struct siplog_private *)wi->loginfo->private;
    if (private->fd >= 0) {
	offset = siplog_lockf(private->fd);
	if (wi->idx_id[0] != '\0') {
	    siplog_update_index(wi->idx_id, private->fd, offset);
	}
	write(private->fd, wi->data, wi->len);
	siplog_unlockf(private->fd, offset);
    }
}

static void
siplog_queue_handle_close(struct siplog_wi *wi)
{
    struct siplog_private *private;

    private = (struct siplog_private *)wi->loginfo->private;
    if (private->fd >= 0) {
        close(private->fd);
        private->fd = -1;
    }
}

static void
siplog_queue_handle_owrc(struct siplog_wi *wi)
{

    siplog_queue_handle_open(wi);
    siplog_queue_handle_write(wi);
    siplog_queue_handle_close(wi);
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
		siplog_queue_handle_close(wi);
		/* free loginfo structure */
                free(wi->loginfo->private);
		siplog_free(wi->loginfo);
		break;

	    case SIPLOG_ITEM_ASYNC_OWRC:
		siplog_queue_handle_owrc(wi);
		break;

            case SIPLOG_ITEM_ASYNC_EXIT:
                return;

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

    pthread_cond_init(&siplog_queue_cond, NULL);
    pthread_mutex_init(&siplog_queue_mutex, NULL);
    pthread_cond_init(&siplog_wi_free_cond, NULL);
    pthread_mutex_init(&siplog_wi_free_mutex, NULL);

    if (pthread_create(&siplog_queue, NULL, (void *(*)(void *))&siplog_queue_run, NULL) != 0)
	return -1;

    return 0;
}

int
siplog_logfile_async_open(struct loginfo *lp)
{
    struct siplog_wi *wi;
    struct siplog_private *private;

    pthread_mutex_lock(&siplog_init_mutex);
    if (siplog_queue_inited == 0) {
	if (siplog_queue_init() != 0) {
	    pthread_mutex_unlock(&siplog_init_mutex);
	    return -1;
	}
    }
    siplog_queue_inited = 1;

    if (atexit_registered == 0) {
	atexit(siplog_logfile_async_atexit);
	atexit_registered = 1;
    }
    if (atfork_registered == 0) {
	pthread_atfork(siplog_logfile_async_atfork, NULL, NULL);
	atfork_registered = 1;
    }
    pthread_mutex_unlock(&siplog_init_mutex);

    private = malloc(sizeof(*private));
    if (private == NULL)
        return -1;

    memset(&private, 0, sizeof(*private));
    private->fd = -1;

    lp->private = (void *)private;

    if ((lp->flags & LF_REOPEN) == 0) {
	wi = siplog_queue_get_free_item(SIPLOG_WI_NOWAIT);
	if (wi == NULL) {
            free(lp->private);
	    return -1;
        }

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
  const char *idx_id, const char *fmt, va_list ap)
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
	if (s2 >= s1) {
	    /* message was truncated */
	    p[s1 - 2] = '\n';
	    break;
	}
	p += s2;
	s1 -= s2;

	s2 = vsnprintf(p, s1, fmt, ap);
	if (s2 >= s1) {
	    /* message was truncated */
	    p[s1 - 2] = '\n';
	    break;
	}
	p += s2;
	s1 -= s2;

	if (estr != NULL) {
	    s2 = snprintf(p, s1, ": %s", estr);
	    if (s2 >= s1) {
		/* message was truncated */
		p[s1 - 2] = '\n';
		break;
	    }
	    p += s2;
	    s1 -= s2;
	}

	if (s1 == 1) {
	    /* message was truncated */
	    p -= 1;
	}

	p[0] = '\n';
	p[1] = '\0';

    } while (0);

    if (idx_id != NULL) {
        strlcpy(wi->idx_id, idx_id, sizeof(wi->idx_id));
    } else {
        wi->idx_id[0] = '\0';
    }

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
