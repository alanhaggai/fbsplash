#!/bin/bash
# A simple script to take screenshots of fbsplash themes.
# (c) 2007, Michal Januszewski <spock@gentoo.org>

# A temporary theme name.  There should be no existing
# fbsplash theme
tmptheme="tmp123456"

splash_send() {
	echo "$*" > /lib/splash/cache/.splash
}

# Save the original fb geometry
xres=$(fbset | grep geometry | awk '{print $2}')
yres=$(fbset | grep geometry | awk '{print $3}')

# Set a standard resolution on all consoles
sx=1024
sy=768

fbset -a -xres ${sx} -yres ${sy}

. /sbin/splash-functions.sh
splash_setup

for i in unpacked/* ; do
	theme=${i//unpacked\//}

	if [ ! -d "unpacked/${theme}" -o ! -e "unpacked/${theme}/metadata.xml" ]; then
		continue
	fi

	rm -rf "/etc/splash/${tmptheme}"
	mkdir /etc/splash/${tmptheme}
	cp -r unpacked/${theme}/* /etc/splash/${tmptheme}/

	# Fix broken config files using absolute path names.
	sed -i -re 's#/+etc/splash/+[^/]*/##' /etc/splash/${tmptheme}/*.cfg

	export RUNLEVEL="S"
	export SOFTLEVEL=""
	export SPLASH_THEME="${tmptheme}"
	export SPLASH_XRES=${sx}
	export SPLASH_YRES=${sy}

	if [ -x "/etc/splash/${tmptheme}/scripts/rc_init-pre" ]; then
		/etc/splash/${tmptheme}/scripts/rc_init-pre sysinit
	fi

	if [ -x "/etc/splash/${tmptheme}/scripts/rc_init-post" ]; then
		/etc/splash/${tmptheme}/scripts/rc_init-post sysinit
	fi

	fbsplashd -t ${tmptheme}

	if [ "$?" != "0" ]; then
		echo "Failed to load theme '${theme}'." >&2
		fbset -xres ${xres} -yres ${yres}
		exit 1
	fi

	# FIXME: script event hooks should be used for update_svc below.
	# Simulate the initial part of the boot sequence.
	splash_send "set message Booting the system (\$progress%)..."
	splash_send "progress 16390"
	splash_send "update_svc clock svc_inactive_start"
	splash_send "update_svc device-mapper svc_inactive_start"
	splash_send "update_svc lvm svc_inactive_start"
	splash_send "update_svc checkroot svc_inactive_start"
	splash_send "update_svc modules svc_inactive_start"
	splash_send "update_svc checkfs svc_inactive_start"
	splash_send "update_svc localmount svc_inactive_start"
	splash_send "update_svc hostname svc_inactive_start"
	splash_send "update_svc hibernate-cleanup svc_inactive_start"
	splash_send "update_svc bootmisc svc_inactive_start"
	splash_send "update_svc alsasound svc_inactive_start"
	splash_send "update_svc dbus svc_inactive_start"
	splash_send "update_svc bluetooth svc_inactive_start"
	splash_send "update_svc keymaps svc_inactive_start"
	splash_send "update_svc consolefont svc_inactive_start"
	splash_send "update_svc net.lo svc_inactive_start"
	splash_send "update_svc net.eth0 svc_inactive_start"
	splash_send "update_svc rmnologin svc_inactive_start"
	splash_send "update_svc udev-postmount svc_inactive_start"
	splash_send "update_svc urandom svc_inactive_start"
	splash_send "update_svc syslog-ng svc_inactive_start"
	splash_send "update_svc acpid svc_inactive_start"
	splash_send "update_svc fbcondecor svc_inactive_start"
	splash_send "update_svc gpm svc_inactive_start"
	splash_send "update_svc xdm svc_inactive_start"
	splash_send "update_svc cpufrequtils svc_inactive_start"
	splash_send "update_svc hddtemp svc_inactive_start"
	splash_send "update_svc net.wlan0 svc_inactive_start"
	splash_send "update_svc spamd svc_inactive_start"
	splash_send "update_svc postfix svc_inactive_start"
	splash_send "update_svc sshd svc_inactive_start"
	splash_send "update_svc vixie-cron svc_inactive_start"
	splash_send "update_svc local svc_inactive_start"
	splash_send "update_svc clock svc_started"
	splash_send "update_svc device-mapper svc_started"
	splash_send "update_svc lvm svc_started"
	splash_send "update_svc bootmisc svc_started"
	splash_send "update_svc alsasound svc_started"
	splash_send "update_svc checkroot svc_started"
	splash_send "update_svc hostname svc_started"
	splash_send "update_svc modules svc_started"
	splash_send "update_svc checkfs svc_started"
	splash_send "update_svc localmount svc_started"
	splash_send "update_svc keymaps svc_started"
	splash_send "set mode silent"
	splash_send "repaint"

	sleep 1
	fbgrab "shots/${sx}x${sy}-${theme}.png"

	splash_send "exit"

	export RUNLEVEL="3"

	if [ -x "/etc/splash/${tmptheme}/scripts/rc_exit-pre" ]; then
		/etc/splash/${tmptheme}/scripts/rc_exit-pre default
	fi

	if [ -x "/etc/splash/${tmptheme}/scripts/rc_exit-post" ]; then
		/etc/splash/${tmptheme}/scripts/rc_exit-post default
	fi

	# Take a screenshot of the verbose splash (fbcondecor)
	if [ -e /etc/splash/${tmptheme}/${sx}x${sy}.cfg -a \
		 -n "$(grep '^pic=' /etc/splash/${tmptheme}/${sx}x${sy}.cfg)" -o \
		 -n "$(grep '^jpeg=' /etc/splash/${tmptheme}/${sx}x${sy}.cfg)" ]; then
		fbcondecor_ctl -t ${tmptheme} --tty=16 -c setcfg
		fbcondecor_ctl -t ${tmptheme} --tty=16 -c on
		chvt 16
		sleep 1
		top -n 1 >/dev/tty16
		fbgrab "shots/${sx}x${sy}-${theme}-fbcondecor.png"
		chvt 1
	fi
done

# Restore the initial resolution
fbset -a -xres ${xres} -yres ${yres}

