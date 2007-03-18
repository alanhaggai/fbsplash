# Copyright 1999-2006 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

# Author:     Michal Januszewski <spock@gentoo.org>
# Maintainer: Michal Januszewski <spock@gentoo.org>

# This file is a part of splashutils.

# FIXME: handle services order on shutdown..

# ####################################################################
#    Change any settings ONLY if you are sure what you're doing.
#    Don't cry if it breaks afterwards.
# ####################################################################

# The splash scripts need a cache which can be guaranteed to be
# both readable and writable at all times, even when the root fs
# is mounted read-only. To that end, an in-RAM fs is used. Valid
# values for spl_cachetype are 'tmpfs' and 'ramfs'. spl_cachesize
# is a size limit in KB, and it should probably be left with the
# default value.
spl_util="/sbin/splash_util.static"
spl_bindir="/lib/splash/bin"
spl_cachedir="/lib/splash/cache"
spl_tmpdir="/lib/splash/tmp"
spl_cachesize="4096"
spl_cachetype="tmpfs"
spl_fifo="${spl_cachedir}/.splash"
spl_pidfile="${spl_cachedir}/daemon.pid"

. /etc/init.d/functions.sh
splash_setup

# This is the main function which handles all events.
# Accepted parameters:
#  svc_start <name>
#  svc_stop <name>
#  svc_started <name> <errcode>
#  svc_stopped <name> <errcode>
#  svc_input_begin <name>
#  svc_input_end <name>
#  rc_init <internal_runlevel> - used to distinguish between 'boot' and 'sysinit'
#  rc_exit
#  critical
splash() {
	local event="$1"
	shift

	[ ${SPLASH_MODE_REQ} == "off" ] && return

	case "$event" in
		svc_input_begin)	splash_input_begin "$1";;
		svc_input_end)		splash_input_end "$1";;
		critical) 			splash_verbose;;
	esac

	return 0
}

splash_setup() {
	# If it's already set up, let's not waste time on parsing the config
	# files again
	if [ ${SPLASH_THEME} != "" -a ${SPLASH_TTY} != "" -a "$1" != "force" ]; then
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

# ----------------------------------------------------------------------
# RUNLEVEL   SOFTLEVEL         INTERNAL    SVCS
# ----------------------------------------------------------------------
# System boot-up:
#    S        <none>           sysinit     CRITICAL_SERVICES
#    S        boot             <none>	   boot_serv - CRITICAL_SERVICES
#    3        default          <none>      std
#
# System restart/shutdown:
#   0/6       reboot/shutdown  <none>      all

# Start the splash daemon.

splash_start() {
	# Display a warning if the system is not configured to display init messages
	# on tty1. This can cause a lot of problems if it's not handled correctly, so
	# we don't allow silent splash to run on incorrectly configured systems.
	if [ ${SPLASH_MODE_REQ} == "silent" ]; then
		if [ -z "`grep -E '(^| )CONSOLE=/dev/tty1( |$)' /proc/cmdline`" -a
			 -z "`grep -E '(^| )console=tty1( |$)' /proc/cmdline`" ]; then
			clear
			ewarn "You don't appear to have a correct console= setting on your kernel"
			ewarn "command line. Silent splash will not be enabled. Please add"
			ewarn "console=tty1 or CONSOLE=/dev/tty1 to your kernel command line"
			ewarn "to avoid this message."
			if [[ -n "`grep 'CONSOLE=/dev/tty1' /proc/cmdline`" ||
				  -n "`grep 'console=tty1' /proc/cmdline`" ]]; then
				ewarn "Note that CONSOLE=/dev/tty1 and console=tty1 are general parameters and"
				ewarn "not splash= settings."
			fi
			return 1
		fi

		mount -n --bind / ${spl_tmpdir}
		if [ ! -c "${spl_tmpdir}/dev/tty1" ]; then
			umount -n ${spl_tmpdir}
			ewarn "The filesystem mounted on / doesn't contain the /dev/tty1 device"
			ewarn "which is required for the silent splash to function properly."
			ewarn "Silent splash will not be enabled. Please create the appropriate"
			ewarn "device node to avoid this message."
			return 1
		fi
		umount -n ${spl_tmpdir}
	fi

	# In the unlikely case that there's a splash daemon running -- kill it.
	killall -9 ${spl_util##*/} 2>/dev/null
	rm -f "${spl_pidfile}"

	# Prepare the communications FIFO
	if [ ! -p "${spl_fifo}" ]; then
		rm -f "${spl_fifo}" 2>/dev/null
		mkfifo "${spl_fifo}"
	fi

	local options=""
	[[ ${SPLASH_KDMODE} == "GRAPHICS" ]] && options="--kdgraphics"

	# Start the splash daemon
	BOOT_MSG="$(splash_get_boot_message)" ${spl_util} -d --theme=${SPLASH_THEME} --pidfile=${spl_pidfile} ${options}
	return 0
}

splash_get_boot_message() {
	if [[ ${RUNLEVEL} == "6" ]]; then
		echo "${SPLASH_REBOOT_MESSAGE}"
	elif [[ ${RUNLEVEL} == "0" ]]; then
		echo "${SPLASH_SHUTDOWN_MESSAGE}"
	else
		echo "${SPLASH_BOOT_MESSAGE}"
	fi
}

###########################################################################
# Common functions
###########################################################################

# Sends data to the splash FIFO after making sure there's someone
# alive on other end to receive it.
splash_comm_send() {
	if [ ! -e ${spl_pidfile} ]; then
		return 1
	fi

	splash_profile "comm $*"

	if [ -r /proc/$(<${spl_pidfile})/status -a
		  "$((read t;echo ${t/Name:/}) </proc/$(<${spl_pidfile})/status)" == "splash_util.sta" ]; then
		echo "$*" > ${spl_fifo} &
	else
		echo "Splash daemon not running!"
		rm -f "${spl_pidfile}"
	fi
}

# Returns the current splash mode.
splash_get_mode() {
	local ctty="$(${spl_bindir}/fgconsole)"

	if [ ${ctty} == "${SPLASH_TTY}" ]; then
		echo "silent"
	else
		if [ -z "$(${spl_util} -c getstate --vc=$(($ctty-1)) 2>/dev/null | grep off)" ]; then
			echo "verbose"
		else
			echo "off"
		fi
	fi
}

# Switches to verbose mode.
splash_verbose() {
	if [ -x /usr/bin/chvt ]; then
		/usr/bin/chvt 1
	else
		splash_comm_send "set mode verbose"
	fi
}

# Switches to silent mode.
splash_silent() {
	splash_comm_send "set mode silent"
	${spl_util} -c on 2>/dev/null
}

# Saves profiling information
splash_profile() {
	if [[ ${SPLASH_PROFILE} == "on" ]]; then
		echo "`cat /proc/uptime | cut -f1 -d' '`: $*" >> ${spl_cachedir}/profile
	fi
}

###########################################################################
# Service
###########################################################################

# args: <svc>
splash_input_begin() {
	local svc="$1"

	if [[ "$(splash_get_mode)" == "silent" ]] ; then
		splash_verbose
		export SPL_SVC_INPUT_SILENT=${svc}
	fi
}

# args: <svc>
splash_input_end() {
	local svc="$1"

	if [[ ${SPL_SVC_INPUT_SILENT} == "${svc}" ]]; then
		splash_silent
		unset SPL_SVC_INPUT_SILENT
	fi
}

###########################################################################
# Cache
###########################################################################

# args: <start|stop>
#
# Prepare the splash cache.
splash_cache_prep() {
	# Mount an in-RAM filesystem at spl_tmpdir.
	mount -ns -t "${spl_cachetype}" cachedir "${spl_tmpdir}" \
		-o rw,mode=0644,size="${spl_cachesize}"k

	retval=$?

	if [[ ${retval} -ne 0 ]]; then
		eerror "Unable to create splash cache - switching to verbose."
		splash_verbose
		return "${retval}"
	fi

	mount -n --move "${spl_tmpdir}" "${spl_cachedir}"

	if [[ "$1" == "start" ]]; then
		echo $(splash_svclist_update "start") > ${spl_cachedir}/svcs_start
		echo -n > ${spl_cachedir}/profile
	fi

	return 0
}

splash_cache_cleanup() {
	# If we don't care about the splash profiling data, or if
	# we're running off a livecd, simply ymount the splash cache.
	if [ -n "${CDBOOT}" -o "${SPLASH_PROFILE}" != "on" ]; then
		umount -l "${spl_cachedir}" 2>/dev/null
		return
	fi

	# Don't try to clean anything up if the cachedir is not mounted.
	[ -z "$(grep ${spl_cachedir} /proc/mounts)" ] && return;

	# Create the temp dir if necessary.
	if [ ! -d "${spl_tmpdir}" ]; then
		mkdir -p "${spl_tmpdir}" 2>/dev/null
		[ "$?" != "0" ] && return
	fi

	# If the /etc is not writable, don't update /etc/mtab. If it is
	# writable, update it to avoid stale mtab entries (bug #121827).
	local mntopt=""
	[ -w /etc/mtab ] || mntopt="-n"
	mount "${mntopt}" --move "${spl_cachedir}" "${spl_tmpdir}" 2>/dev/null

	# Don't try to copy anything if the cachedir is not writable.
	[ -w "${spl_cachedir}" ] || return;

	cp -a "${spl_tmpdir}"/profile "${spl_cachedir}" 2>/dev/null
	umount -l "${spl_tmpdir}" 2>/dev/null
 }

###########################################################################
# Service list
###########################################################################

# args: <internal-runlevel>
splash_svclist_init() {
	arg="$1"

	if [[ ${SOFTLEVEL} == "reboot" || ${SOFTLEVEL} == "shutdown" ]]; then
		for i in `dolisting "${svcdir}/started/" | sed -e "s#${svcdir}/started/##g"`; do
			splash_svc_update ${i} "svc_inactive_stop"
		done
	elif [[ ${RUNLEVEL} == "S" ]]; then
		local svcs=$(splash_svclist_get start)

		if [[ ${arg} == "sysinit" ]]; then
			for i in ${svcs} ; do
				splash_svc_update ${i} "svc_inactive_start"
			done
		fi
	fi
}

splash_svclist_get() {
	if [[ "$1" == "start" ]]; then
		cat ${spl_cachedir}/svcs_start
	fi
}

splash_svclist_update() {
	local svcs= order= x= dlvl="${SOFTLEVEL}"

	for x in $(dolisting /etc/runlevels/${BOOTLEVEL}) \
		$(dolisting ${svcdir}/coldplugged) ; do
		svcs="${svcs} ${x##*/}"
		if [[ ${x##*/} == "autoconfig" ]] ; then
			svcs="${svcs} $(. /etc/init.d/autoconfig; list_services)"
		fi
	done
	order=$(rc-depend -ineed -iuse -iafter ${svcs})

	# We call rc-depend twice so we get the ordering exactly right
	svcs=
	if [[ ${SOFTLEVEL} == "${BOOTLEVEL}" ]] ; then
		dlvl="${DEFAULTLEVEL}"
	fi
	for x in $(dolisting /etc/runlevels/"${dlvl}") ; do
		svcs="${svcs} ${x##*/}"
		if [[ ${x##*/} == "autoconfig" ]] ; then
			svcs="${svcs} $(. /etc/init.d/autoconfig; list_services)"
		fi
	done
	order="${order} $(SOFTLEVEL="$dlvl" rc-depend -ineed -iuse -iafter ${svcs})"

	# Only list each service once
	uniqify ${order}
}

###########################################################################
# Other
###########################################################################

# Override sulogin calls from baselayout so that we can attempt to remove
# the splash screen
sulogin() {
       splash "critical" > /dev/null 2>&1 &
       /sbin/sulogin $*
}

# vim:ts=4
