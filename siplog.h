/* $Id$ */

#ifndef _SIPLOG_H_
#define _SIPLOG_H_

#ifndef _SIPLOG_T_DEFINED
typedef void * siplog_t;
#define _SIPLOG_T_DEFINED
#endif

#define SIPLOG_DBUG	0
#define SIPLOG_INFO	1
#define SIPLOG_WARN	2
#define SIPLOG_ERR	3
#define SIPLOG_CRIT	4

#define	SIPLOG_ALL	SIPLOG_INFO	/* XXX */

#define LF_REOPEN	1

#include <stdarg.h>	/* Needed for the va_list */

#ifdef __cplusplus
extern "C" {
#endif

siplog_t siplog_open(const char *app, const char *call_id, int flags);
int	 siplog_set_level(siplog_t handle, int level);
#define	 siplog_get_level(handle) siplog_set_level((handle), -1)
void	 siplog_write(int level, siplog_t handle, const char *format, ...);
void	 siplog_write_va(int level, siplog_t handle, const char *format, va_list);
void	 siplog_ewrite(int level, siplog_t handle, const char *format, ...);
void	 siplog_ewrite_va(int level, siplog_t handle, const char *format, va_list);
void	 siplog_iwrite(int level, siplog_t handle, const char *, const char *format, ...);
void	 siplog_close(siplog_t handle);

int      siplog_memdeb_dumpstats(int level, siplog_t handle);
void     siplog_memdeb_setbaseln(void);

#ifdef __cplusplus
}
#endif

#endif /* _SIPLOG_H_ */
