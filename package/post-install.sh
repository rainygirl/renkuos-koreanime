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
#    the debugger -- keyboard and mouse dead until a reboot. So input_server
#    is frozen first, the files are moved, and it is killed, which runs no
#    destructor; app_server starts it again at once, without HanBe.
#
# 2. Korean becomes the system language unless it already is (koreanime-setup
#    leaves a Korean system alone). At first boot after an offline install
#    the registrar may not be up yet -- koreanime-setup then exits with 2 --
#    so keep trying for a minute, in the background, not to hold up the boot.

disabled="$HOME/config/settings/koreanime/disabled"
method_dirs="$HOME/config/non-packaged/add-ons/input_server/methods
$HOME/config/add-ons/input_server/methods
/boot/system/non-packaged/add-ons/input_server/methods"

others=""
for dir in $method_dirs; do
	for method in "$dir"/HanBe*; do
		[ -e "$method" ] && others="$others
$method"
	done
done

if [ -n "$others" ]; then
	mkdir -p "$disabled"
	# not awk: not every system has it
	server=$(ps | while read -r command pid rest; do
		[ "$command" = /boot/system/servers/input_server ] && echo "$pid"
	done)
	[ -n "$server" ] && kill -STOP $server 2> /dev/null
	printf '%s\n' "$others" | while IFS= read -r method; do
		[ -n "$method" ] && mv "$method" "$disabled/" 2> /dev/null
	done
	[ -n "$server" ] && kill -KILL $server 2> /dev/null
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
