#include <stdbool.h>

/* Default TTYs for silent and verbose modes. */
#define TTY_SILENT		8
#define TTY_VERBOSE		1

#define DEFAULT_THEME	"default"

typedef struct
{
	char reqmode;	/* requested splash mode */
	char *theme;	/* theme */
	int kdmode;		/* KD_TEXT or KD_GRAPHICS */
	int tty_s;		/* silent tty */
	int tty_v;		/* verbose tty */
	bool profile;	/* enable profiling? */
} scfg_t;

int splash_config_init(scfg_t *cfg);
int splash_parse_kcmdline(scfg_t *cfg);

enum sp_states { st_display, st_svc_inact_start, st_svc_inact_stop, st_svc_start,
				 st_svc_started, st_svc_stop, st_svc_stopped, st_svc_stop_failed,
				 st_svc_start_failed };



