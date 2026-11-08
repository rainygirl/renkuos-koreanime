#!/bin/sh
# Run by the package kit once koreanime is installed.
#
# 1. Other Korean input methods -- HanBe, installed by hand as an add-on file
#    -- would sit beside this one in the Deskbar menu and in the Alt+Space
#    cycle, so they are moved out of input_server's folders (not deleted:
#    they are kept in ~/config/settings/koreanime/disabled).
#
#    Not by simply moving the file: input_server notices and unloads the
#    add-on, HanBe's destructor frees memory twice, and input_server stops in
#    the debugger -- keyboard and mouse dead until a reboot. So input_server's
#    add-on monitor thread is frozen, the files are moved, and input_server is
#    killed, which runs no destructor; app_server starts it again at once,
#    without HanBe.
#
#    Only that thread is frozen, not the whole team: a SIGSTOP to the team
#    also hits the mouse thread, blocked reading the PS/2 mouse, and the
#    killed team then leaves the mouse open in the kernel -- the new
#    input_server cannot open it and the pointer stays frozen.
#
# 2. Korean becomes the system language unless it already is (koreanime-setup
#    leaves a Korean system alone). At first boot after an offline install
#    the registrar may not be up yet -- koreanime-setup then exits with 2 --
#    so keep trying for a minute, in the background, not to hold up the boot.

disabled="$HOME/config/settings/koreanime/disabled"
method_dirs="$HOME/config/non-packaged/add-ons/input_server/methods
$HOME/config/add-ons/input_server/methods
/boot/system/non-packaged/add-ons/input_server/methods"

input_server_pid() {
	# not awk: not every system has it
	ps | while read -r command pid rest; do
		[ "$command" = /boot/system/servers/input_server ] && echo "$pid"
	done
}

# input_server's add-on monitor thread, the one that would see the files go.
add_on_monitor() {
	ps -a | while read -r name id rest; do
		case "$name" in
		/boot/system/servers/input_server) inside=1 ;;
		/*) inside= ;;
		AddOnMonitor) [ -n "$inside" ] && echo "$id" ;;
		esac
	done
}

others=""
for dir in $method_dirs; do
	for method in "$dir"/HanBe*; do
		[ -e "$method" ] && others="$others
$method"
	done
done

if [ -n "$others" ]; then
	mkdir -p "$disabled"
	server=$(input_server_pid)
	[ -n "$server" ] && kill -STOP $server 2> /dev/null
	printf '%s\n' "$others" | while IFS= read -r method; do
		[ -n "$method" ] && mv "$method" "$disabled/" 2> /dev/null
	done
	[ -n "$server" ] && kill -KILL $server 2> /dev/null
	[ -n "$server" ] && check_pointer $server
fi

(
	tries=0
	while [ $tries -lt 30 ]; do
		koreanime-setup > /dev/null 2>&1
		[ $? -ne 2 ] && break
		tries=$((tries + 1))
		sleep 2
	done
) > /dev/null 2>&1 &
exit 0
