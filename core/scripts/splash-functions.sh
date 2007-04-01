# Copyright 1999-2007 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

# Author:     Michal Januszewski <spock@gentoo.org>
# Maintainer: Michal Januszewski <spock@gentoo.org>

# This file is a part of splashutils. The functions contained in this
# file are meant to be used by hook scripts in splash themes. The code
# will be kept distro-agnostic to facilitate portability.

# ####################################################################
#    Change any settings ONLY if you are sure what you're doing.
#    Don't cry if it breaks afterwards.
# ####################################################################

# The splash scripts need a cache which can be guaranteed to be
# both readable and writable at all times, even when the root fs
# is mounted read-only. This writable space is provided by a tmpfs
# mounted at ${spl_cachedir}.
spl_util="/sbin/splash_util.static"
spl_bindir="/lib/splash/bin"
spl_cachedir="/lib/splash/cache"
spl_fifo="${spl_cachedir}/.splash"
spl_pidfile="${spl_cachedir}/daemon.pid"

[ -r /etc/init.d/functions.sh ] && . /etc/init.d/functions.sh

splash_setup() {
	# If it's already set up, let's not waste time on parsing the config
	# files again
	if [ "${SPLASH_THEME}" != "" -a "${SPLASH_TTY}" != "" -a "$1" != "force" ]; then
		return 0
	fi

	export SPLASH_MODE_REQ="off"
	export SPLASH_PROFILE="off"
	export SPLASH_THEME="default"
	export SPLASH_TTY="16"
	export SPLASH_KDMODE="TEXT"
	export SPLASH_BOOT_MESSAGE="Booting the system (\$progress%)... Press F2 for verbose mode."
	export SPLASH_SHUTDOWN_MESSAGE="Shutting down the system (\$progress%)... Press F2 for verbose mode."
	export SPLASH_REBOOT_MESSAGE="Rebooting the system (\$progress%)... Press F2 for verbose mode."

	[ -f /etc/conf.d/splash ] && . /etc/conf.d/splash

	if [ -f /proc/cmdline ]; then
		options=$(grep 'splash=[^ ]*' -o /proc/cmdline)

		# Execute this loop over $options so that we can process multiple
		# splash= arguments on the kernel command line. Useful for adjusting
		# splash parameters from ISOLINUX.
		for opt in ${options} ; do
			options=${opt#*=}

			for i in ${options//,/ } ; do
				case ${i%:*} in
					theme)		SPLASH_THEME=${i#*:} ;;
					tty)		SPLASH_TTY=${i#*:} ;;
					verbose) 	SPLASH_MODE_REQ="verbose" ;;
					silent)		SPLASH_MODE_REQ="silent" ;;
					kdgraphics)	SPLASH_KDMODE="GRAPHICS" ;;
					profile)	SPLASH_PROFILE="on" ;;
				esac
			done
		done
	fi
}

splash_get_boot_message() {
	if [ "${RUNLEVEL}" == "6" ]; then
		echo "${SPLASH_REBOOT_MESSAGE}"
	elif [ "${RUNLEVEL}" == "0" ]; then
		echo "${SPLASH_SHUTDOWN_MESSAGE}"
	else
		echo "${SPLASH_BOOT_MESSAGE}"
	fi
}

###########################################################################
# Common functions
###########################################################################

# Sends data to the splash FIFO after making sure there's someone
# alive on the other end to receive it.
splash_comm_send() {
	if [ ! -e "${spl_pidfile}" ]; then
		return 1
	fi

	if [ -r /proc/$(<"${spl_pidfile}")/status -a
		  "$((read t;echo ${t/Name:/}) </proc/$(<${spl_pidfile})/status)" == "splash_util.sta" ]; then
		echo "$*" > "${spl_fifo}" &
	else
		echo "Splash daemon not running!"
		rm -f "${spl_pidfile}"
	fi
}

# Returns the current splash mode.
splash_get_mode() {
	local ctty="$(${spl_bindir}/fgconsole)"

	if [ "${ctty}" == "${SPLASH_TTY}" ]; then
		echo "silent"
	else
		if [ -z "$(${spl_util} -c getstate --vc=$(($ctty-1)) 2>/dev/null | grep off)" ]; then
			echo "verbose"
		else
			echo "off"
		fi
	fi
}

# chvt <n>
# --------
# Switches to the n-th tty.
chvt() {
	local ntty=$1

	if [ -x /usr/bin/chvt ] ; then
		/usr/bin/chvt ${ntty}
	else
		echo -en "\e[12;${ntty}]"
	fi
}

# Switches to verbose mode.
splash_verbose() {
	chvt 1
}

# Switches to silent mode.
splash_silent() {
	splash_comm_send "set mode silent"
}

###########################################################################
# Service list
###########################################################################

# splash_svclist_get <type>
# -------------------------
# type:
#  - start - to get a list of services to be started during bootup
#  - stop  - to get a list of services to be stopped during shutdown/reboot
splash_svclist_get() {
	if [ "$1" == "start" -a -r "${spl_cachedir}/svcs_start" ]; then
		cat "${spl_cachedir}/svcs_start"
	elif [ "$1" == "stop" -a -r "${spl_cachedir}/svcs_stop"]; then
		cat "${spl_cachedir}/svcs_stop"
	fi
}

splash_setup

# vim:ts=4
