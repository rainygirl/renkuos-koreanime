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
#    add-on monitor thread is suspended (koreanime-setup --hold-add-ons), the
#    files are moved, and input_server is killed, which runs no destructor;
#    app_server starts it again at once, without HanBe.
#
#    Only that thread is held, not the whole team, and the pointing devices
#    are stopped first, so input_server closes the mouse itself. A SIGSTOP to
#    the team, or a kill while the mouse thread is reading, can leave the
#    PS/2 mouse open in the kernel: the new input_server cannot open it and
#    the pointer stays frozen.
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
	# suspends the add-on monitor thread (the shell's kill takes teams only)
	# and prints input_server's team
	server=$(koreanime-setup --hold-add-ons 2> /dev/null)
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
