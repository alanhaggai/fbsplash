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
	splash_setup

	[[ ${SPLASH_MODE_REQ} == "off" ]] && return

	# Prepare the cache here -- rc_init-pre might want to use it
	if [[ ${event} == "rc_init" ]]; then
		if [[ ${RUNLEVEL} == "S" && "$2" == "sysinit" ]]; then
			splash_cache_prep 'start' || return
		elif [[ ${SOFTLEVEL} == "reboot" || ${SOFTLEVEL} == "shutdown" ]]; then
			# Check if the splash cachedir is mounted readonly. If it is,
			# we need to mount a tmpfs over it.
			if ! touch ${spl_cachedir}/message 2>/dev/null ; then
				splash_cache_prep 'stop' || return
			fi
		fi
	fi
	
	# Handle -pre event hooks
	if [[ -x "/etc/splash/${SPLASH_THEME}/scripts/${event}-pre" ]]; then
		/etc/splash/${SPLASH_THEME}/scripts/${event}-pre "$2" "$3"
	fi
			
	case "$event" in
		svc_start)			splash_svc_start "$2";;
		svc_stop)			splash_svc_stop "$2";;
		svc_started) 		splash_svc "$2" "$3" "start";;
		svc_stopped)		splash_svc "$2" "$3" "stop";;
		svc_input_begin)	splash_input_begin "$2";;
		svc_input_end)		splash_input_end "$2";;
		rc_init) 			splash_init "$2";;
		rc_exit) 			splash_exit;;
		critical) 			splash_verbose;;
	esac
	
	# Handle -post event hooks
	if [[ -x "/etc/splash/${SPLASH_THEME}/scripts/${event}-post" ]]; then
		/etc/splash/${SPLASH_THEME}/scripts/${event}-post "$2" "$3"
	fi
	
	return 0
}
 
splash_setup() {
	# If it's already set up, let's not waste time on parsing the config 
	# files again
	if [[ ${SPLASH_THEME} != "" && ${SPLASH_TTY} != "" && "$1" != "force" ]]; then
		return 0
	fi
	
	export SPLASH_MODE_REQ="off"
	export SPLASH_THEME="default"
	export SPLASH_TTY="16"
	export SPLASH_KDMODE="TEXT"
	export SPLASH_BOOT_MESSAGE="Booting the system (\$progress%)... Press F2 for verbose mode."
	export SPLASH_SHUTDOWN_MESSAGE="Shutting down the system (\$progress%)... Press F2 for verbose mode."
	export SPLASH_REBOOT_MESSAGE="Rebooting the system (\$progress%)... Press F2 for verbose mode."

	[[ -f $(add_suffix /etc/conf.d/splash) ]] && source "$(add_suffix /etc/conf.d/splash)"
		
	if [[ -f /proc/cmdline ]]; then
		options=$(grep 'splash=[^ ]*' -o /proc/cmdline)
		options=${options#*=}
	
		for i in ${options//,/ } ; do
			case ${i%:*} in
				theme)		SPLASH_THEME=${i#*:} ;;
				tty)		SPLASH_TTY=${i#*:} ;;
				verbose) 	SPLASH_MODE_REQ="verbose" ;;
				silent)		SPLASH_MODE_REQ="silent" ;;
				kdgraphics)	SPLASH_KDMODE="GRAPHICS" ;;
			esac
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

# args: <internal_runlevel>
#
# This function is called when an 'rc_init' event takes place,
# ie. when the runlevel is changed. 
splash_init() {
	arg="$1"

	# Initialize variables -- either set the default values or load them from a file
	if [[ ${RUNLEVEL} == "S" && ${arg} == "sysinit" ]] ||
	   [[ ${SOFTLEVEL} == "reboot" || ${SOFTLEVEL} == "shutdown" ]]; then
		spl_count=0
		spl_scripts=0
		spl_execed=""
 	else
		splash_load_vars
	fi

	export spl_count spl_scripts spl_execed
	
	if [[ ${RUNLEVEL} == "S" && ${arg} == "sysinit" ]]; then
		spl_scripts=$(splash_svclist_get start | tr ' ' '\n' | wc -l)
		spl_count=0
	elif [[ ${SOFTLEVEL} == "reboot" || ${SOFTLEVEL} == "shutdown" ]]; then
		spl_started=($(dolisting "${svcdir}/started/"))
		spl_scripts=${#spl_started[*]}
	fi

	if [[ ${RUNLEVEL} == "S" && ${arg} == "sysinit" ]] || 
	   [[ ${SOFTLEVEL} == "reboot" || ${SOFTLEVEL} == "shutdown" ]]; then
		splash_start
	fi
	
	splash_svclist_init "${arg}"
	splash_save_vars
}

# args: none
#
# This function is called when an 'rc_exit' event takes place, 
# ie. when we're almost done with executing initscripts for a
# given runlevel.
splash_exit() 
	# If we're in sysinit or rebooting, do nothing.
	if [[ ${RUNLEVEL} == "S" || ${SOFTLEVEL} == "reboot" || ${SOFTLEVEL} == "shutdown" ]]; then
		return 0
	fi

	if [[ "$(splash_get_mode)" == "silent" ]] ; then
		splash_verbose
	fi

	splash_comm_send "exit"
	splash_cache_cleanup

	# Make sure the splash daemon is really dead (just in case the killall
	# in splash_cache_cleanup didn't get executed). This should fix Gentoo
	# bug #96697.
	killall -9 splash_util.static >/dev/null 2>/dev/null
}

splash_start() {
	# Prepare the communications FIFO
	rm -f ${spl_fifo} 2>/dev/null
		
	if [[ ${SPLASH_MODE_REQ} == "verbose" ]]; then
		${spl_util} -c on 2>/dev/null
		return 0	
	elif [[ ${SPLASH_MODE_REQ} != "silent" ]]; then
		return 0
	fi
		
	# Display a warning if the system is not configured to display init messages
	# on tty1. This can cause a lot of problems if it's not handled correctly, so
	# we don't allow silent splash to run on incorrectly configured systems.
	if [[ ${SPLASH_MODE_REQ} == "silent" ]]; then
		if [[ -z "`grep -E '(^| )CONSOLE=/dev/tty1( |$)' /proc/cmdline`" &&
			  -z "`grep -E '(^| )console=tty1( |$)' /proc/cmdline`" ]]; then
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

		mount --bind / ${spl_tmpdir}
		if [[ ! -c "${spl_tmpdir}/dev/tty1" ]]; then
			umount ${spl_tmpdir}
			clear
			ewarn "The filesystem mounted on / doesn't contain the /dev/tty1 device"
			ewarn "which is required for the silent splash to function properly."
			ewarn "Silent splash will not be enabled. Please create the appropriate"
			ewarn "device file to avoid this message."
			return 1
		fi
		umount ${spl_tmpdir}
	fi

	# In the unlikely case that there's a splash daemon running -- kill it.
	killall -9 ${spl_util##*/} 2>/dev/null
	
	# Prepare the communications FIFO
	mkfifo ${spl_fifo}

	local options=""
	[[ ${SPLASH_KDMODE} == "GRAPHICS" ]] && options="--kdgraphics"
		
	# Start the splash daemon
	${spl_util} -d --theme=${SPLASH_THEME} --pidfile=${spl_pidfile} ${options}

	# Set the silent TTY and boot message
	splash_comm_send "set tty silent ${SPLASH_TTY}"
	splash_comm_send "set message $(splash_get_boot_message)"
	
	if [[ ${SPLASH_MODE_REQ} == "silent" ]] ; then
		splash_comm_send "set mode silent"
		splash_comm_send "repaint"
		${spl_util} -c on 2>/dev/null
	fi

	# Set the input device if it exists. This will make it possible to use F2 to 
	# switch from verbose to silent.
	local t=$(grep -Hsi keyboard /sys/class/input/event*/device/driver/description | grep -o 'event[0-9]\+') 
	if [[ -z "${t}" ]]; then
		# Try an alternative method of finding the event device. The idea comes
		# from Bombadil <bombadil(at)h3c.de>. We're couting on the keyboard controller
		# being the first device handled by kbd listed in input/devices.
		t=$(/bin/grep -s -m 1 '^H: Handlers=kbd' /proc/bus/input/devices | grep -o 'event[0-9]*')
	fi
	[[ -n "${t}" ]] && splash_comm_send "set event dev /dev/input/${t}"

	return 0
}

splash_get_boot_message() {
	if [[ ${RUNLEVEL} == "6" ]]; then
		echo ${SPLASH_REBOOT_MESSAGE}
	elif [[ ${RUNLEVEL} == "0" ]]; then
		echo ${SPLASH_SHUTDOWN_MESSAGE}
	else
		echo ${SPLASH_BOOT_MESSAGE}
	fi	
}

splash_update_progress() {
	local srv=$1
		
	# FIXME
	splash_load_vars	
	[[ -n "${spl_execed}" && -z "${spl_execed//* $srv */}" ]] && return
	[[ -z "${spl_scripts}" ]] && return
	spl_execed="${spl_execed} ${srv} "
	spl_count=$((${spl_count} + 1))
	
	if [ "${spl_scripts}" -gt 0 ]; then
		progress=$(($spl_count * (65535 - $spl_init) / $spl_scripts))
	else
		progress=0
	fi

	splash_comm_send "progress ${progress}"
	splash_save_vars
	splash_comm_send "paint"
}

###########################################################################
# Common functions
###########################################################################

# Sends data to the splash FIFO after making sure there's someone
# alive on other end to receive it.
splash_comm_send() {
	if [[ ! -e ${spl_pidfile} ]]; then
		return 1
	fi
	
	if [[ -r /proc/$(<${spl_pidfile})/status && 
		  "$((read t;echo ${t/Name:/}) </proc/$(<${spl_pidfile})/status)" == "splash_util.sta" ]]; then
		echo $* > ${spl_fifo} &
	fi
}

# Returns the current splash mode. 
splash_get_mode() {
	local ctty="$(${spl_bindir}/fgconsole)"

	if [[ ${ctty} == "${SPLASH_TTY}" ]]; then
		echo "silent"
	else
		if [[ -z "$(${spl_util} -c getstate --vc=$(($ctty-1)) 2>/dev/null | grep off)" ]]; then
			echo "verbose"
		else
			echo "off"
		fi	
	fi
}	

# Switches to verbose mode.
splash_verbose() {
	if [[ -x /usr/bin/chvt ]]; then
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

splash_load_vars() {
	[[ -e ${spl_cachedir}/progress ]] && source ${spl_cachedir}/progress
}

splash_save_vars() {
	if [[ ! -d ${spl_cachedir} || ! -w ${spl_cachedir} ]]; then
		return
	fi
		
	t="spl_execed=\"${spl_execed}\"\n"
	t="${t}spl_count=${spl_count}\n"
	t="${t}spl_scripts=${spl_scripts}\n"
	
	(echo -e "$t" > ${spl_cachedir}/progress) 2>/dev/null
}

###########################################################################
# Service
###########################################################################

# args: <svc> <error-code> <action>
splash_svc() {
	local srv="$1"
	local err="$2"
	local act="$3"

	# We ignore the serial initscript since it's known to return bogus error codes
	# while not printing any error messages. This only confuses the users.
	if [[ ${err} -ne 0 && ${SPLASH_VERBOSE_ON_ERRORS} == "yes" && "${srv}" != "serial" ]]; then
		splash_verbose
		return 1
	fi

	if [[ ${act} == "start" ]]; then
		if [[ ${err} -eq 0 ]]; then
			splash_svc_update ${srv} "svc_started"
		else
			splash_svc_update ${srv} "svc_start_failed"
		fi
	else
		if [[ ${err} -eq 0 ]]; then
			splash_svc_update ${srv} "svc_stopped"
		else
			splash_svc_update ${srv} "svc_stop_failed"
		fi
	fi

	splash_update_progress "$srv"
}

# args: <svc> <state>
#
# Inform the splash daemon about service status changes.
splash_svc_update() {
	splash_comm_send "update_svc $1 $2"
}

# args: <svc>
splash_svc_start() {
	local svc="$1"

	splash_svc_update ${svc} "svc_start"
	splash_update_progress "${svc}"
}

# args: <svc>
splash_svc_stop() {
	local svc="$1"

	splash_svc_update ${svc} "svc_stop"
	splash_update_progress "${svc}"
}

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
	
	# Copy the dependency cache and services lists to our new cache dir.
	# With some luck, we won't have to update it.
	cp -a ${svcdir}/{depcache,deptree} "${spl_tmpdir}" 2>/dev/null
	cp -a ${spl_cachedir}/{svcs_start,svcs_stop,levels} "${spl_tmpdir}" 2>/dev/null

	# Now that the data from the old cache is copied, move tmpdir to cachedir.
	mount -n --move "${spl_tmpdir}" "${spl_cachedir}"

	h=$(stat -c '%y' ${spl_cachedir}/deptree 2>/dev/null)

	# Point depscan.sh to our cachedir
	/sbin/depscan.sh --svcdir "${spl_cachedir}"

	if [[ "$1" == "start" ]]; then
		# Check whether the list of services that will be started during boot
		# needs updating. This is generally the case if:
		#  - one of the caches doesn't exist
		#  - our deptree was out of date
		#  - we're booting with a different boot/default level than the last time
		#  - one of the runlevel dirs has been modified since the last boot
		if [[ ! -e ${spl_cachedir}/levels || \
			  ! -e ${spl_cachedir}/svcs_start ]]; then
			echo $(splash_svclist_update "start") > ${spl_cachedir}/svcs_start
		else
			local lastlev timestamp
			{ read lastlev; read timestamp; } < ${spl_cachedir}/levels
			if [[ "${lastlev}" != "${BOOTLEVEL}/${DEFAULTLEVEL}" || \
				  "${timestamp}" != "$(stat -c '%y' /etc/runlevels/${BOOTLEVEL})/$(stat -c '%y' /etc/runlevels/${DEFAULTLEVEL})" || \
				  "$(stat -c '%y' ${spl_cachedir}/deptree)" != "${h}" ]]; then
				echo $(splash_svclist_update "start") > ${spl_cachedir}/svcs_start
			fi
		fi
	fi
		
	return 0
}


splash_cache_cleanup() {
	# FIXME: Make sure the splash daemon is dead.
	killall -9 splash_util.static >/dev/null 2>/dev/null
	umount -l "${spl_tmpdir}" 2>/dev/null

	# There's no point in saving all the data if we're running off a livecd.
	[[ -n "${CDBOOT}" ]] && return;

	# Don't try to clean anything up if the cachedir is not mounted.
	[[ -z "$(grep ${spl_cachedir} /proc/mounts)" ]] && return;
	
	# Create the temp dir if necessary.
	if [[ ! -d "${spl_tmpdir}" ]]; then
		mkdir -p "${spl_tmpdir}" 2>/dev/null
		[[ "$?" != "0" ]] && return
	fi
	
	# If the /etc is not writable, don't update /etc/mtab. If it is 
	# writable, update it to avoid stale mtab entries (bug #121827).
	local mntopt=""
	[[ -w /etc/mtab ]] || mntopt="-n"
	mount ${mntopt} --move "${spl_cachedir}" "${spl_tmpdir}" 2>/dev/null

	# Don't try to copy anything is the cachedir is not writable.
	[[ -w "${spl_cachedir}" ]] || return;

	cp -a "${spl_tmpdir}"/{envcache,depcache,deptree,svcs_start,svcs_stop} "${spl_cachedir}" 2>/dev/null
	echo "${BOOTLEVEL}/${DEFAULTLEVEL}" > "${spl_cachedir}/levels"
	echo "$(stat -c '%y' /etc/runlevels/${BOOTLEVEL})/$(stat -c '%y' /etc/runlevels/${DEFAULTLEVEL})" \
			 >> "${spl_cachedir}/levels"
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
	(
	# Source our own deptree and required functions
	source ${spl_cachedir}/deptree
	[[ ${RC_GOT_SERVICES} != "yes" ]] && source "${svclib}/sh/rc-services.sh"

	svcs_started=" "
	svcs_order=""

	# We're sure our depcache is up-to-date, no need to waste 
	# time checking mtimes.
	check_mtime() { 
		return 0 
	}
	
	is_net_up() {
		local netcount=0

		case "${RC_NET_STRICT_CHECKING}" in
			lo)
				netcount="$(echo ${svcs_started} | tr ' ' '\n' | \
			            egrep -c "\/net\..*$")"
				;;
			*)
				netcount="$(echo ${svcs_started} | tr ' ' '\n' | \
			            grep -v 'net\.lo' | egrep -c "\/net\..*$")"
				;;
		esac

		# Only worry about net.* services if this is the last one running,
		# or if RC_NET_STRICT_CHECKING is set ...
		if [[ ${netcount} -lt 1 || ${RC_NET_STRICT_CHECKING} == "yes" ]]; then
			return 1
		fi

		return 0
	}

	service_started() {
		if [[ -z "${svcs_started/* ${1} */}" ]]; then
			return 0	
		else
			return 1
		fi
	}

	# This simulates the service startup and has to mimic the behaviour of 
	# svc_start() from /sbin/runscript.sh and start_service() from rc-functions.sh
	# as closely as possible.
	start_service() {
		local svc="$1"
	
		if service_started ${svc}; then
			return
		fi

		# Prevent recursion..
		svcs_started="${svcs_started}$1 "

	   	if is_fake_service "${svc}" "${mylevel}"; then
			svcs_order="${svcs_order} ${svc}"
			return
		fi
		
		svcs_startup="$(ineed "${svc}") \
					$(valid_iuse "${svc}") \
					$(valid_iafter "${svc}")"
		
		for x in ${svcs_startup} ; do
			if [[ ${x} == "net" ]] && [[ ${svc%%.*} != "net" || ${svc##*.} == ${svc} ]] && ! is_net_up ; then
				local netservices="$(dolisting "/etc/runlevels/${BOOTLEVEL}/net.*") \
						$(dolisting "/etc/runlevels/${mylevel}/net.*")"
	
				for y in ${netservices} ; do
					mynetservice="${y##*/}"
					if ! service_started ${mynetservice} ; then
						start_service "${mynetservice}"
					fi
				done
			else
				if ! service_started ${x} ; then
					start_service "${x}"
				fi
			fi
		done 
	
		svcs_order="${svcs_order} ${svc}"
		return
	}

	# This function should return a list of services that will be started
	# from /etc/init.d/autoconfig. In order to do that, we source 
	# /etc/init.d/autoconfig and use its list_services() function.
	autoconfig_svcs() {
		[[ -r /etc/init.d/autoconfig ]] || return
		. /etc/init.d/autoconfig
		echo "$(list_services)"
	}
	
	as="$(autoconfig_svcs)"
	[[ -n "${SOFTLEVEL}" ]] && ss="$(dolisting "/etc/runlevels/${SOFTLEVEL}/") "
	sb="$(dolisting "/etc/runlevels/${BOOTLEVEL}/") "
	sd="$(dolisting "/etc/runlevels/${DEFAULTLEVEL}/") "

	# If autoconfig is one of the services that will be started,
	# insert an updated list of services into our list.
	if [[ -z "${ss/*\/autoconfig */}" ]]; then
		ss="${ss/\/autoconfig /\/autoconfig $as }"
	fi
		
	if [[ -z "${sb/*\/autoconfig */}" ]]; then
		sb="${sb/\/autoconfig /\/autoconfig $as }"
	fi

	if [[ -z "${sd/*\/autoconfig */}" ]]; then
		sd="${sd/\/autoconfig /\/autoconfig $as }"
	fi
	
	mylevel="${BOOTLEVEL}"
	for i in ${CRITICAL_SERVICES} ${sb}; do
		start_service "${i##*/}"
	done
	mylevel="${DEFAULTLEVEL}"
	for i in ${LOGGER_SERVICE} ${sd} ${ss}; do
		start_service "${i##*/}"
	done
	echo "${svcs_order}"
	)
}

# vim:ts=4
