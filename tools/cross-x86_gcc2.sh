#!/bin/sh
# Cross-builds the x86_gcc2 add-on and setup tool with gcc 2.95, inside a
# container that holds a configured RenkuOS x86_gcc2 build tree (the flags are
# the ones jam uses for RenkuOS's own t9 input method).
#
#   tools/cross-x86_gcc2.sh [generated dir] [source tree]
#
# Run from the repository root; writes dist/x86_gcc2/.
set -e

G="${1:-/vaio-p/work/generated.x86_gcc2h}"
H="${2:-/vaio-p/work/renku}"
CROSS="$G/cross-tools-x86_gcc2"
CC="$CROSS/bin/i586-pc-haiku-gcc"
GCCLIB="$(ls -d $CROSS/lib/gcc-lib/i586-pc-haiku/2.95.3-*)"
O="$G/objects/haiku/x86_gcc2/release"
LIBGCC="$(ls $G/build_packages/gcc_syslibs_devel-*/develop/lib/libgcc.a | head -1)"
OUT=dist/x86_gcc2

INCLUDES="-I$H/build/user_config_headers -I$H/build/config_headers -I-
	-I$H/headers/private/input -I$H/headers/private/interface
	-I$H/headers/private/locale -I$H/headers/private/shared
	-I$H/headers/private/app -I$H/headers/private/.
	-I$H/headers/cpp -I$H/headers/glibc -I$H/headers/posix
	-I$H/headers/build/gcc-2.95.3 -I$H/headers -I$H/headers/os"
for d in add-ons add-ons/input_server app device interface kernel locale \
		storage support; do
	INCLUDES="$INCLUDES -I$H/headers/os/$d"
done

CFLAGS="-O2 -Wall -Wno-multichar -pipe -march=pentium -nostdinc -DARCH_x86
	-D__HAIKU_PRIMARY_PACKAGING_ARCH=\"x86_gcc2\" -DHAIKU_TARGET_PLATFORM_HAIKU
	-Isrc"

mkdir -p "$OUT" build/x86_gcc2
for f in Hangul KoreanInputMethod KoreanSetup; do
	$CC -c src/$f.cpp $CFLAGS $INCLUDES -o build/x86_gcc2/$f.o
done

# the input method: an add-on loaded by input_server
$CC -pipe -march=pentium -shared -nostdlib -Xlinker --no-undefined \
	-Xlinker -soname=KoreanIME -o "$OUT/KoreanIME" \
	"$O/system/glue/arch/x86/crti.o" "$GCCLIB/crtbeginS.o" \
	"$O/system/glue/init_term_dyn.o" \
	build/x86_gcc2/Hangul.o build/x86_gcc2/KoreanInputMethod.o \
	"$O/kits/libbe.so" "$O/servers/input/input_server" \
	"$O/system/libroot/libroot.so" "$LIBGCC" \
	"$GCCLIB/crtendS.o" "$O/system/glue/arch/x86/crtn.o"

# koreanime-setup: switches the system language to Korean
$CC -pipe -march=pentium -nostdlib -Xlinker --no-undefined \
	-Xlinker -soname=_APP_ -o "$OUT/koreanime-setup" \
	"$O/system/glue/arch/x86/crti.o" "$GCCLIB/crtbeginS.o" \
	"$O/system/glue/start_dyn.o" "$O/system/glue/init_term_dyn.o" \
	build/x86_gcc2/KoreanSetup.o \
	"$O/kits/libbe.so" "$O/system/libroot/libroot.so" "$LIBGCC" \
	"$GCCLIB/crtendS.o" "$O/system/glue/arch/x86/crtn.o"

ls -l "$OUT"
