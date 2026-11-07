#!/bin/sh
# Run by the package kit once koreanime is installed: makes Korean the system
# language unless it already is (koreanime-setup leaves a Korean system
# alone). At first boot after an offline install the registrar may not be up
# yet -- koreanime-setup then exits with 2 -- so keep trying for a minute,
# in the background so as not to hold up the boot.
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
