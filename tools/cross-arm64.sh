#!/bin/sh
# Cross-builds the arm64 add-on and setup tool with gcc 13, inside a
# container that holds a configured RenkuOS arm64 build tree (the flags are
# the ones jam uses for RenkuOS's own t9 input method).
#
#   tools/cross-arm64.sh [generated dir] [source tree]
#
# Run from the repository root; writes dist/arm64/.
set -e

G="${1:-/root/renku-arm64-work/generated.arm64}"
H="${2:-/root/renku-arm64-work/haiku}"
CROSS="$G/cross-tools-arm64"
CC="$CROSS/bin/aarch64-unknown-haiku-gcc"
GCCLIB="$(ls -d $CROSS/lib/gcc/aarch64-unknown-haiku/*)"
O="$G/objects/haiku/arm64/release"
SYSLIBS="$(ls -d $G/build_packages/gcc_syslibs-*-arm64 | head -1)"
SYSDEVEL="$(ls -d $G/build_packages/gcc_syslibs_devel-*-arm64 | head -1)"
OUT=dist/arm64

INCLUDES="-I$H/headers/private/input -I$H/headers/private/interface
	-I$H/headers/private/locale -I$H/headers/private/shared
	-I$H/headers/private/app -I$H/headers/private/.
	-I$SYSDEVEL/develop/headers/c++
	-I$SYSDEVEL/develop/headers/c++/aarch64-unknown-haiku
	-I$H/headers/glibc -I$H/headers/posix
	-I$SYSDEVEL/develop/headers/gcc/include
	-I$SYSDEVEL/develop/headers/gcc/include-fixed
	-I$H/headers -I$H/headers/os"
for d in add-ons add-ons/input_server app device interface kernel locale \
		storage support; do
	INCLUDES="$INCLUDES -I$H/headers/os/$d"
done

CFLAGS="-O2 -Wall -Wno-multichar -pipe -march=armv8-a+crc -nostdinc
	-fno-strict-aliasing -DARCH_arm64
	-D__HAIKU_PRIMARY_PACKAGING_ARCH=\"arm64\" -DHAIKU_TARGET_PLATFORM_HAIKU
	-Isrc"

LIBS="$SYSLIBS/lib/libstdc++.so $O/system/libroot/libroot.so
	$SYSLIBS/lib/libgcc_s.so.1 $SYSDEVEL/develop/lib/libgcc.a"

mkdir -p "$OUT" build/arm64
for f in Hangul KoreanInputMethod KoreanSetup; do
	$CC -c src/$f.cpp $CFLAGS $INCLUDES -o build/arm64/$f.o
done

$CC -pipe -shared -nostdlib -Xlinker --no-undefined \
	-Xlinker -soname=KoreanIME -o "$OUT/KoreanIME" \
	"$O/system/glue/arch/arm64/crti.o" "$GCCLIB/crtbeginS.o" \
	"$O/system/glue/init_term_dyn.o" \
	build/arm64/Hangul.o build/arm64/KoreanInputMethod.o \
	"$O/kits/libbe.so" "$O/servers/input/input_server" $LIBS \
	"$GCCLIB/crtendS.o" "$O/system/glue/arch/arm64/crtn.o"

$CC -pipe -nostdlib -Xlinker --no-undefined \
	-Xlinker -soname=_APP_ -o "$OUT/koreanime-setup" \
	"$O/system/glue/arch/arm64/crti.o" "$GCCLIB/crtbeginS.o" \
	"$O/system/glue/start_dyn.o" "$O/system/glue/init_term_dyn.o" \
	build/arm64/KoreanSetup.o \
	"$O/kits/libbe.so" $LIBS \
	"$GCCLIB/crtendS.o" "$O/system/glue/arch/arm64/crtn.o"

ls -l "$OUT"
