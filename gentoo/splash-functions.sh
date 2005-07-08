# Copyright 1999-2005 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

# Author: Michal Januszewski <spock@gentoo.org>
# Maintainer: Michal Januszewski <spock@gentoo.org>

# This file is a part of splashutils.

# FIXME: handle services order on shutdown..

# ####################################################################
#    Change any settings ONLY when you are sure what you're doing.
#    Don't try if it breaks afterwards.
# ####################################################################

# The splash scripts need a cache which can be guaranteed to be
# both readable and writable at all times, even when the root fs
# is mounted read-only. To that end, a in-RAM fs is used. Valid
# values for spl_cachetype are 'tmpfs' and 'ramfs'. spl_cachesize
# is a size limit in KB, and it should probably be left with the
# default value.
spl_cachedir="/lib/splash/cache"
spl_tmpdir="/lib/splash/tmp"
spl_cachesize="4096"
spl_cachetype="tmpfs"
spl_fifo="${spl_cachedir}/.splash"
spl_pidfile="${spl_cachedir}/daemon.pid"

# This is a little tricky. We need depscan.sh to create an updated cache for
# us, and we need it in our place ($spl_cachedir), and not in $svcdir, since the
# latter might not be writable at this moment. In order to get what we need, 
# we trick depscan.sh into thinking $svcdir is $spl_cachedir. 
#
# Here is how it works:
# - $svcdir is defined in /sbin/functions.sh
# - /sbin/splash-functions.sh is sourced from /sbin/function.sh, after $svcdir
#   is defined
# - /sbin/functions.sh is sourced from /sbin/depscan.sh
#
# $spl_cache_depscan is used to prevent breaking things when /sbin/depscan.sh
# is not run by us.
if [[ "$0" == "/sbin/depscan.sh" && "${spl_cache_depscan}" == "yes" ]]; then
	svcdir="${spl_cachedir}"
fi 
	
# This is the main function that handles all events.
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

	if [[ ${SPLASH_MODE_REQ} == "off" ]]; then
		return
	fi

	# Prepare the cache here - rc_init-pre might want to use it
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
	
	if [[ -f /etc/conf.d/splash ]]; then 
		. /etc/conf.d/splash
	fi
		
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
splash_init() {
	arg="$1"

	# Initialize variables - either set the default values or load them from a file
	if [[ ${RUNLEVEL} == "S" && ${arg} == "sysinit" ]] ||
	   [[ ${SOFTLEVEL} == "reboot" || ${SOFTLEVEL} == "shutdown" ]]; then
		spl_init=0
		spl_count=0
		spl_scripts=0
		spl_rate=65535
		spl_execed=""
 	else
		splash_load_vars
	fi

	export spl_init spl_count spl_scripts spl_rate spl_execed
	
	if [[ ${RUNLEVEL} == "S" && ${arg} == "sysinit" ]]; then
		spl_scripts=$(splash_svclist_get start | tr ' ' '\n' | wc -l)
		spl_count=0
	elif [[ ${SOFTLEVEL} == "reboot" || ${SOFTLEVEL} == "shutdown" ]]; then
		spl_started=($(dolisting "${svcdir}/started/"))
		spl_scripts=${#spl_started[*]}
	fi

	if [[ ${RUNLEVEL} == "S" && ${arg} == "sysinit" ]] || 
	   [[ ${SOFTLEVEL} == "reboot" || ${SOFTLEVEL} == "shutdown" ]]; then
		/sbin/splash "start"
		# Set the input device if it exists
		local t=$(grep -Hsi keyboard /sys/class/input/event*/device/driver/description | grep -o 'event[0-9]\+') 
		if [[ -n "${t}" ]]; then
			splash_comm_send "set event dev /dev/input/${t}"
		fi
	fi
	
	splash_init_svclist "${arg}"
	splash_save_vars
}

splash_cache_prep() {
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

	mount -n --move "${spl_tmpdir}" "${spl_cachedir}"

	h=$(stat -c '%y' ${spl_cachedir}/deptree 2>/dev/null)

	# Point depscan.sh to our cachedir
	spl_cache_depscan="yes" /sbin/depscan.sh -u

	if [[ "$1" == "start" ]]; then
		# Check whether the list of services that will be started during boot
		# needs updating. This is generally the case if:
		#  - one of the caches doesn't exist
		#  - out deptree was out of date
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
	# Don't try to clean anything up if the cachedir is not mounted.
	[[ -z "$(grep ${spl_cachedir} /proc/mounts)" ]] && return;
	[[ ! -d "${spl_tmpdir}" ]] && mkdir "${spl_tmpdir}"
	mount -n --move "${spl_cachedir}" "${spl_tmpdir}"

	# There's no point in saving all this data if we're running off a livecd.
	if [[ -z "${CDBOOT}" ]]; then
		cp -a "${spl_tmpdir}"/{envcache,depcache,deptree,svcs_start,svcs_stop} "${spl_cachedir}" 2>/dev/null
		echo "${BOOTLEVEL}/${DEFAULTLEVEL}" > "${spl_cachedir}/levels"
		echo "$(stat -c '%y' /etc/runlevels/${BOOTLEVEL})/$(stat -c '%y' /etc/runlevels/${DEFAULTLEVEL})" \
			 >> "${spl_cachedir}/levels"
	fi

	# Make sure the splash daemon is dead.
	killall -9 splash_util.static >/dev/null 2>/dev/null
	umount -l "${spl_tmpdir}" 2>/dev/null
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

	# This functions should return a list of services that will be started
	# from /etc/init.d/autoconfig. In order to do that, we have to do
	# kernel command line parsing, just as it's done in the autoconfig
	# script.
	autoconfig_svcs() {
		GPM="yes"
		HOTPLUG="yes"
		CMDLINE="`cat /proc/cmdline`"
		asvcs=""
		
		for x in ${CMDLINE} ; do
			if [ "${x}" == "nodetect" ]; then
				GPM="no"
				HOTPLUG="no"
			fi
			if [[ ${x} == "apci=on" || ${x} == "acpi=force" ]] && [[ -x /etc/init.d/acpid ]]; then
				asvcs="${asvcs} acpid"
			elif [[ ${x} == "doapm" && -x /etc/init.d/apmd ]]; then
				asvcs="${asvcs} apmd"
			fi
			if [[ ${x} == "dopcmcia" && -x /etc/init.d/pcmcia ]]; then
				asvcs="${asvcs} pcmcia"
			fi
			if [[ ${x} == "nogpm" ]]; then
				GPM="no"
			fi
			if [[ ${x} == "nohotplug" ]]; then
				HOTPLUG="no"
			fi
		done

		[[ ${GPM} == "yes" && -x /etc/init.d/gpm ]] && asvcs="${asvcs} gpm"
		if [[ ${HOTPLUG} == "yes" ]]; then
			if [[ -x /etc/init.d/coldplug ]]; then
				asvcs="${asvcs} coldplug"
			elif [[ -x /etc/init.d/hotplug ]]; then
				asvcs="${asvcs} hotplug"
			fi
		fi

		echo "${asvcs}"
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

splash_svc() {
	local srv="$1"
	local err="$2"
	local act="$3"

	# We ignore the serial initscript since it's known to return bogus error codes
	# while not printing any error messages. This only confuses the users.
	if [[ ${err} -ne 0 && ${SPLASH_VERBOSE_ON_ERRORS} == "yes" && "${srv}" != "serial" ]]; then
		/sbin/splash "verbose"
		return 1
	fi

	if [[ ${act} == "start" ]]; then
		if [[ ${err} -eq 0 ]]; then
			splash_update_svc ${srv} "svc_started"
		else
			splash_update_svc ${srv} "svc_start_failed"
		fi
	else
		if [[ ${err} -eq 0 ]]; then
			splash_update_svc ${srv} "svc_stopped"
		else
			splash_update_svc ${srv} "svc_stop_failed"
		fi
	fi

	/sbin/splash "$srv"
}

splash_exit() {
	if [[ ${RUNLEVEL} == "S" || ${SOFTLEVEL} == "reboot" || ${SOFTLEVEL} == "shutdown" ]]; then
		return 0
	fi

	if [[ "$(splash_get_mode)" == "silent" ]] ; then
		/sbin/splash "verbose"
	fi

	splash_comm_send "exit"
	splash_cache_cleanup
}

# <svc> <state>
splash_update_svc() {
	local svc=$1
	local state=$2
	splash_comm_send "update_svc ${svc} ${state}"
}

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

splash_get_mode() {
	local ctty="$(/lib/splash/bin/fgconsole)"

	if [[ ${ctty} == "${SPLASH_TTY}" ]]; then
		echo "silent"
	else
		if [[ -z "$(/sbin/splash_util.static -c getstate --vc=$(($ctty-1)) 2>/dev/null | grep off)" ]]; then
			echo "verbose"
		else
			echo "off"
		fi	
	fi
}	

splash_verbose() {
	/sbin/splash "verbose"
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
	t="${t}spl_rate=${spl_rate}\n"
	t="${t}spl_init=${spl_init}\n"
	t="${t}SPL_SVC_INACTIVE_START=\"${SPL_SVC_INACTIVE_START}\"\n"
	t="${t}SPL_SVC_START=\"${SPL_SVC_START}\"\n"
	t="${t}SPL_SVC_STARTED=\"${SPL_SVC_STARTED}\"\n"
	t="${t}SPL_SVC_START_FAILED=\"${SPL_SVC_START_FAILED}\"\n"
	t="${t}SPL_SVC_INACTIVE_STOP=\"${SPL_SVC_INACTIVE_STOP}\"\n"
	t="${t}SPL_SVC_STOP=\"${SPL_SVC_STOP}\"\n"
	t="${t}SPL_SVC_STOPPED=\"${SPL_SVC_STOPPED}\"\n"
	t="${t}SPL_SVC_STOP_FAILED=\"${SPL_SVC_STOP_FAILED}\"\n"
	
	(echo -e "$t" > ${spl_cachedir}/progress) 2>/dev/null
}

splash_input_begin() {
	local svc="$1"

	if [[ "$(splash_get_mode)" == "silent" ]] ; then
		/sbin/splash "verbose"
		export SPL_SVC_INPUT_SILENT=${svc}
	fi
}

splash_input_end() {
	local svc="$1"

	if [[ ${SPL_SVC_INPUT_SILENT} == "${svc}" ]]; then
		/sbin/splash "silent"
		unset SPL_SVC_INPUT_SILENT	
	fi
}

splash_svc_start() {
	local svc="$1"

	splash_update_svc ${svc} "svc_start"
	/sbin/splash "$svc"
}

splash_svc_stop() {
	local svc="$1"

	splash_update_svc ${svc} "svc_stop"
	/sbin/splash "$svc"
}

splash_init_svclist() {
	arg="$1"
	
	if [[ ${SOFTLEVEL} == "reboot" || ${SOFTLEVEL} == "shutdown" ]]; then
		for i in `dolisting "${svcdir}/started/" | sed -e "s#${svcdir}/started/##g"`; do
			splash_update_svc ${i} "svc_inactive_stop"
		done
	elif [[ ${RUNLEVEL} == "S" ]]; then
		local svcs=$(splash_svclist_get start)
		
		if [[ ${arg} == "sysinit" ]]; then
			for i in ${svcs} ; do
				splash_update_svc ${i} "svc_inactive_start"
			done
		fi
	fi
}

# vim:ts=4
