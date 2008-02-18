#ifndef __DAEMON_H
#define __DAEMON_H

#include "common.h"
#include "render.h"
#include <pthread.h>
#include <time.h>

/* daemon.c */
void obj_update_status(char *svc, enum ESVC state);
int reload_theme(void);

#define UPD_SILENT	0x01
#define UPD_MON		0x02
#define UPD_ALL		(UPD_SILENT | UPD_MON)
void switchmon_start(int update, int stty);

extern stheme_t *theme;
extern u8 *fb_mem;
extern int fd_fb;

#define ALRM_INTERRUPT   0
#define ALRM_AUTOVERBOSE 1
extern int alarm_type;

/*
 * Current TTY. This effectively identifies the current splash
 * mode.
 */
#define CTTY_SILENT		0
#define CTTY_VERBOSE	1
extern int ctty;

/*
 * Silent and verbose TTYs. We keep a file descriptor for the silent
 * TTY and for tty1.
 */
extern pthread_mutex_t mtx_tty;
extern int tty_s, tty_v;
extern int fd_tty_s, fd_tty1, fd_tty0;

/*
 * Event device on which the daemon listens for F2 keypresses.
 * The proper device has to be detected by an external program and
 * then enabled by sending an appropriate command to the splash
 * daemon.
 */
extern int fd_evdev;
extern char *evdev;

#ifdef CONFIG_GPM
#include <gpm.h>
extern int fd_gpm;
#endif

/*
 * Threads
 */
extern pthread_t th_switchmon, th_sighandler, th_anim;
extern pthread_mutex_t mtx_paint;
extern pthread_mutex_t mtx_anim;
extern pthread_cond_t  cnd_anim;


/*
 * Service state structure.
 */
typedef struct {
	struct timespec ts;
	char *svc;
	enum ESVC state;
} svc_state;

/*
 * Services list.
 */
extern list svcs;

/* daemon_cmd.c */
int cmd_update_svc(void **args);
int cmd_set_theme(void **args);
int cmd_set_tty(void **args);
int cmd_set_mode(void **args);
int cmd_set_event_dev(void **args);
int cmd_set_mesg(void **args);
int cmd_set_notify(void **args);
int cmd_paint(void **args);
int cmd_repaint(void **args);
int cmd_paint_rect(void **args);
int cmd_progress(void **args);
int cmd_exit(void **args);
int daemon_comm(FILE *fp);

typedef struct {
	const char *cmd;
	int (*handler)(void**);
	int args;
	char *specs;
} cmdhandler;

extern cmdhandler known_cmds[];

#endif /* __DAEMON_H */
