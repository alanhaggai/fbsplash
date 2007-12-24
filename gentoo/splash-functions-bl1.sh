#
# splash-functions-bl1.sh
# -----------------------
# This is a legacy file for baselayout v1. It won't be extended to
# support any new features. For these, consult the code of the splash
# plugin for baselayout v2.
#
# Distributed under the terms of the GNU General Public License v2
#
# Author:     Michal Januszewski <spock@gentoo.org>
# Maintainer: Michal Januszewski <spock@gentoo.org>

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
spl_cachesize="4096"
spl_cachetype="tmpfs"

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
splash_exit() {
	# If we're in sysinit or rebooting, do nothing.
	if [[ ${RUNLEVEL} == "S" || ${SOFTLEVEL} == "reboot" || ${SOFTLEVEL} == "shutdown" ]]; then
		return 0
	fi

	if service_started "xdm"; then
		splash_comm_send "exit staysilent"
	else
		splash_comm_send "exit"
	fi
	splash_cache_cleanup
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
		progress=$(($spl_count * 65535 / $spl_scripts))
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

splash_warn() {
	ewarn "$*"
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

	h=$(ls -ld --full-time ${spl_cachedir}/deptree | cut -f6,7,8 -d' ' 2>/dev/null)

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
				  "${timestamp}" != "$(ls -ld --full-time /etc/runlevels/${BOOTLEVEL} | cut -f6,7,8 -d' ')/$(ls -ld --full-time /etc/runlevels/${DEFAULTLEVEL} | cut -f6,7,8 -d' ')" || \
				  "$(ls -ld --full-time ${spl_cachedir}/deptree | cut -f6,7,8 -d' ')" != "${h}" ]]; then
				echo $(splash_svclist_update "start") > ${spl_cachedir}/svcs_start
			fi
		fi

		echo -n > ${spl_cachedir}/profile
	fi

	return 0
}


splash_cache_cleanup() {
	rm -f "${spl_pidfile}"

	# There's no point in saving all the data if we're running off a livecd.
	if [[ -n "${CDBOOT}" ]]; then
		umount -l "${spl_cachedir}" 2>/dev/null
		return
	fi

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

	# Don't try to copy anything if the cachedir is not writable.
	[[ -w "${spl_cachedir}" ]] || return;

	cp -a "${spl_tmpdir}"/{envcache,depcache,deptree,svcs_start,svcs_stop,profile} "${spl_cachedir}" 2>/dev/null
	echo "${BOOTLEVEL}/${DEFAULTLEVEL}" > "${spl_cachedir}/levels"
	echo "$(stat -c '%y' /etc/runlevels/${BOOTLEVEL})/$(stat -c '%y' /etc/runlevels/${DEFAULTLEVEL})" \
			 >> "${spl_cachedir}/levels"

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
