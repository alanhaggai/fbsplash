# Copyright 1999-2004 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: $

# Author: Michal Januszewski <spock@gentoo.org>
# Maintainer: Michal Januszewski <spock@gentoo.org>

# This is file is a part of splashutils.

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

splash()
{
	local event="$1"
	splash_setup

	if [ -x "/etc/splash/${SPLASH_THEME}/scripts/${event}-pre" ]; then
		/etc/splash/${SPLASH_THEME}/scripts/${event}-pre "$2" "$3"
	fi
			
	case "$event" in
		svc_start) splash_svc_start "$2";;
		svc_stop) splash_svc_stop "$2";;
		svc_started) splash_svc "$2" "$3" "start";;
		svc_stopped) splash_svc "$2" "$3" "stop";;
		svc_input_begin) splash_input_begin "$2";;
		svc_input_end) splash_input_end "$2";;
		rc_init) splash_init "$2";;
		rc_exit) splash_exit;;
		critical) /sbin/splash "verbose";;
	esac
	
	if [ -x "/etc/splash/${SPLASH_THEME}/scripts/${event}-post" ]; then
		/etc/splash/${SPLASH_THEME}/scripts/${event}-post "$2" "$3"
	fi
	
	return 0
}

splash_setup()
{
	# if it's already setup, let's not waste time on parsing the config files again
	if [ "${SPLASH_THEME}" != "" ]; then
		return 0
	fi
		
	export SPLASH_THEME="default"

	if [ -f /etc/conf.d/splash ]; then 
		. /etc/conf.d/splash
	fi
		
	if [ -f /proc/cmdline ]; then
		# kernel command line override for the splash theme
		for param in `grep "theme:" /proc/cmdline`; do
			t=${param%:*}
			if [ "${t#*,}" == "theme" ]; then
				SPLASH_THEME="${param#*:}"
			fi
		done
	else
		echo "PROC NOT MOUNTED!!!"
	fi
}

splash_svc_start()
{
	local svc="$1"

	splash_load_vars
	SPL_SVC_START="${SPL_SVC_START}${svc} "
	SPL_SVC_INACTIVE_START="${SPL_SVC_INACTIVE_START// $svc / }"
	splash_save_vars

	/sbin/splash "$svc"
}

splash_svc_stop()
{
	local svc="$1"

	splash_load_vars
	SPL_SVC_STOP="${SPL_SVC_STOP}${svc} "
	SPL_SVC_INACTIVE_STOP="${SPL_SVC_INACTIVE_STOP// $svc / }"
	splash_save_vars

	/sbin/splash "$svc"
}

splash_init_svclist()
{
	arg="$1"
	
	export SPL_SVC_INACTIVE_START SPL_SVC_START SPL_SVC_STARTED SPL_SVC_START_FAILED
	export SPL_SVC_INACTIVE_STOP SPL_SVC_STOP SPL_SVC_STOPPED SPL_SVC_STOP_FAILED

	# we don't clear these variables if we have just switched to, for example, runlevel 3
	if [ "${SOFTLEVEL}" == "reboot" -o "${SOFTLEVEL}" == "shutdown" -o "${RUNLEVEL}" == "S" ]; then
		SPL_SVC_INACTIVE_START=" "
		SPL_SVC_START=" "
		SPL_SVC_STARTED=" "
		SPL_SVC_START_FAILED=" "
		SPL_SVC_INACTIVE_STOP=" "
		SPL_SVC_STOP=" "
		SPL_SVC_STOPPED=" "
		SPL_SVC_STOP_FAILED=" "
	fi
		
	if [ "${SOFTLEVEL}" = "reboot" -o "${SOFTLEVEL}" = "shutdown" ]; then
		SPL_SVC_INACTIVE_STOP=" `dolisting "${svcdir}/started/" | sed -e "s#${svcdir}/started/##g"` "
	elif [ "${RUNLEVEL}" == "S" ]; then
		ts="`dolisting "/etc/runlevels/${SOFTLEVEL}/" | sed -e "s#/etc/runlevels/${SOFTLEVEL}/##g"`"
		tb="`dolisting "/etc/runlevels/${BOOTLEVEL}/" | sed -e "s#/etc/runlevels/${BOOTLEVEL}/##g"`"
		td="`dolisting "/etc/runlevels/${DEFAULTLEVEL}/" | sed -e "s#/etc/runlevels/${DEFAULTLEVEL}/##g"`"
		
		if [ "${arg}" == "sysinit" ]; then
			SPL_SVC_INACTIVE_START=" ${CRITICAL_SERVICES} ${ts} ${tb} ${td} "	
		else
			SPL_SVC_STARTED=" `dolisting "${svcdir}/started/" | sed -e "s#${svcdir}/started/##g"` "
			SPL_SVC_INACTIVE_START=" ${ts} ${tb} ${td} "
		
			for i in $SPL_SVC_STARTED; do
				SPL_SVC_INACTIVE_START="${SPL_SVC_INACTIVE_START// $i / }"
			done
		fi
	fi
}

splash_init()
{
	arg="$1"

	# We need this to restore consfont_silent properly, other
	# vars will be overridden in the next few lines
	if [ "${RUNLEVEL}" = "3" ]; then
		splash_load_vars
	fi
	
	spl_init=0
	spl_count=0
	spl_scripts=0
	spl_rate=0
	spl_execed=""

	splash_init_svclist "${arg}"
	
	if [ "${RUNLEVEL}" == "S" ] && [ "${arg}" == "sysinit" ]; then
		temp=($CRITICAL_SERVICES)
		spl_scripts=${#temp[*]}
		spl_rate=16383
	fi

	if [ "${RUNLEVEL}" == "S" -a "${arg}" == "sysinit" ] || 
	   [ "${SOFTLEVEL}" == "reboot" -o "${SOFTLEVEL}" == "shutdown" ]
	then
		/sbin/splash "start"
	fi
	
	export spl_init spl_count spl_scripts spl_rate spl_execed spl_consfont_silent
	splash_save_vars

	if [ "${arg}" != "sysinit" ]; then
		splash_calc
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

splash_calc()
{
	spl_runs=($(dolisting "/etc/runlevels/${SOFTLEVEL}/"))
	spl_runb=($(dolisting "/etc/runlevels/${BOOTLEVEL}/"))

	spl_scripts=${#spl_runs[*]}
	spl_boot=${#spl_runb[*]}

	# In runlevel boot we already have some services started
	if [ "${RUNLEVEL}" = "S" -a "${SOFTLEVEL}" = "boot" ]
	then
		spl_started=($(dolisting "${svcdir}/started/"))
		spl_scripts=$((${spl_boot} - ${#spl_started[*]}))
		spl_init=16383
		spl_rate=26213
	elif [ "${SOFTLEVEL}" = "reboot" -o "${SOFTLEVEL}" = "shutdown" ]
	then
		spl_started=($(dolisting "${svcdir}/started/"))
		spl_scripts=${#spl_started[*]}
		spl_rate=65535
	else
		spl_init=26213
		spl_rate=65535
	fi

	splash_save_vars
}

splash_svc()
{
	local srv="$1"
	local err="$2"
	local act="$3"
	
	splash_load_vars

	[ -e /etc/conf.d/splash ] && source /etc/conf.d/splash
		
	# we ignore consolefont errors because it fails when the console is in KD_GRAPHICS mode
	if [ "${err}" -ne 0 -a "${SPLASH_VERBOSE_ON_ERRORS}" = "yes" -a "${srv}" != "consolefont" ]; then
		/sbin/splash "verbose"
		return 1
	fi

	if [ "${srv}" == "consolefont" ]; then
		splash_is_silent
		export spl_consfont_silent=$?
	fi

	for i in ${spl_execed}
	do
		[ "${i}" = "${srv}" ] && return
	done	

	if [ "${act}" == "start" ]; then
		SPL_SVC_START="${SPL_SVC_START// $srv / }"
		
		if [ "${err}" -eq 0 ]; then
			SPL_SVC_STARTED="${SPL_SVC_STARTED}${srv} "
		else
			SPL_SVC_START_FAIL="${SPL_SVC_START_FAIL}${srv} "
		fi
	else
		SPL_SVC_STOP="${SPL_SVC_STOP// $srv / }"
		
		if [ "${err}" -eq 0 ]; then
			SPL_SVC_STOPPED="${SPL_SVC_STOPPED}${srv} "
		else
			SPL_SVC_STOP_FAIL="${SPL_SVC_STOP_FAIL}${srv} "
		fi
	fi
	
	spl_execed="${spl_execed} ${srv}"
	spl_count=$((${spl_count} + 1))
	/sbin/splash "$srv"
	
	splash_save_vars
}

splash_exit()
{
	if [ "${RUNLEVEL}" = "S" ]; then
		return 0
	fi

	if splash_is_silent ; then
		/sbin/splash "verbose"
	fi
	
	# we need to restart consolefont because fonts don't get set when the vc
	# is in KD_GRAPHICS mode
	if [ -L "${svcdir}/started/consolefont" -a "${spl_consfont_silent}" == "0" ]; then
		/etc/init.d/consolefont restart 2>/dev/null >/dev/null
	fi
}

splash_is_silent()
{
	if [ -n "`/sbin/splash_util -c getmode 2>/dev/null | grep silent`" ]; then
		return 0
	else
		return 1
	fi
}

splash_load_vars()
{
	# In sysinit we don't have the variables, bacause:
	# 1) it's not necessary
	# 2) the root fs is mounted ro
	if [ "${RUNLEVEL}" == "S" -a "${SOFTLEVEL}" != "boot" ]; then
		return 0
	fi 

	[ -e ${svcdir}/progress ] && source ${svcdir}/progress
}

splash_save_vars()
{
	# In sysinit we don't have the variables, bacause:
	# 1) it's not necessary
	# 2) the root fs is mounted ro
	if [ "${RUNLEVEL}" == "S" -a "${SOFTLEVEL}" != "boot" ]; then
		return 0
	fi 
		
	t="spl_execed=\"${spl_execed}\"\n"
	t="${t}spl_count=${spl_count}\n"
	t="${t}spl_scripts=${spl_scripts}\n"
	t="${t}spl_rate=${spl_rate}\n"
	t="${t}spl_init=${spl_init}\n"
	t="${t}spl_consfont_silent=${spl_consfont_silent}\n"
	t="${t}SPL_SVC_INACTIVE_START=\"${SPL_SVC_INACTIVE_START}\"\n"
	t="${t}SPL_SVC_START=\"${SPL_SVC_START}\"\n"
	t="${t}SPL_SVC_STARTED=\"${SPL_SVC_STARTED}\"\n"
	t="${t}SPL_SVC_START_FAILED=\"${SPL_SVC_START_FAILED}\"\n"
	t="${t}SPL_SVC_INACTIVE_STOP=\"${SPL_SVC_INACTIVE_STOP}\"\n"
	t="${t}SPL_SVC_STOP=\"${SPL_SVC_STOP}\"\n"
	t="${t}SPL_SVC_STOPPED=\"${SPL_SVC_STOPPED}\"\n"
	t="${t}SPL_SVC_STOP_FAILED=\"${SPL_SVC_STOP_FAILED}\"\n"
	
	(echo -e "$t" > ${svcdir}/progress) 2>/dev/null
}

splash_input_begin()
{
	local svc="$1"

	if splash_is_silent; then
		/sbin/splash "verbose"
		export SPL_SVC_INPUT_SILENT=${svc}
	fi
}

splash_input_end()
{
	local svc="$1"

	if [ "${SPL_SVC_INPUT_SILENT}" == "${svc}" ]; then
		/sbin/splash "silent"
		unset SPL_SVC_INPUT_SILENT	
	fi
}

# vim:ts=4
