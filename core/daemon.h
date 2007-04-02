#ifndef __DAEMON_H
#define __DAEMON_H

#include "util.h"
#include <pthread.h>

/* daemon.c */
void icon_update_status(char *svc, enum ESVC state);
void bgbuffer_alloc(void);
void free_objs(void);
int reload_theme(void);

#define UPD_SILENT	0x01
#define UPD_MON		0x02
#define UPD_ALL		(UPD_SILENT | UPD_MON)
void switchmon_start(int update);

/*
 * Current TTY. This effectively identifies the current splash
 * mode.
 */
#define CTTY_SILENT		0
#define CTTY_VERBOSE	1
extern int ctty;

/*
 * Notifiers -- external programs to be run when a specific event
 * takes place.
 */
#define NOTIFY_REPAINT	0
#define NOTIFY_PAINT	1
extern char *notify[];

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
 * Framebuffer device and background buffer.
 */
//extern pthread_mutex_t mtx_bgbuf;
extern int fd_fb, fd_bg;
extern u8 *fb_mem, *bg_buffer;

/*
 * Threads
 */
extern pthread_t th_switchmon, th_sighandler, th_anim;
extern pthread_mutex_t mtx_paint;

/*
 * Service state structure.
 */
typedef struct {
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
