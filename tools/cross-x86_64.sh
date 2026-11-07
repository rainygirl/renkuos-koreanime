#!/bin/sh
# Cross-builds the x86_64 add-on, indicator and setup tool with gcc 13 and a
# RenkuOS x86_64 sysroot (system headers, private headers and libraries), as
# found in the haiku-chromium container.
#
#   tools/cross-x86_64.sh [sysroot] [g++]
#
# Run from the repository root; writes dist/x86_64/. The input method is not
# linked against input_server (the sysroot has no servers): its
# BInputServerMethod symbols are left for input_server to provide when it
# loads the add-on.
set -e

Y="${1:-/root/sysroot-x64}"
CXX="${2:-/root/gen-x64/cross-tools-x86_64/bin/x86_64-unknown-haiku-g++}"
H="$Y/boot/system/develop/headers"
OUT=dist/x86_64

FLAGS="--sysroot=$Y -O2 -Wall -Wno-multichar -fPIC -Isrc
	-I$H/private/locale -I$H/private/shared"
LINK="--sysroot=$Y -Wl,--hash-style=both -L$Y/boot/system/develop/lib
	-L$Y/boot/system/lib"

mkdir -p "$OUT" build/x86_64
for f in Hangul KoreanInputMethod KoreanSetup KoreanIndicator; do
	$CXX -c src/$f.cpp $FLAGS -o build/x86_64/$f.o
done

$CXX -shared -o "$OUT/KoreanIME" -Wl,-soname,KoreanIME $LINK \
	build/x86_64/Hangul.o build/x86_64/KoreanInputMethod.o -lbe
$CXX -shared -o "$OUT/KoreanIndicator" -Wl,-soname,KoreanIndicator $LINK \
	-Wl,--no-undefined build/x86_64/KoreanIndicator.o -lbe -lroot
$CXX -o "$OUT/koreanime-setup" $LINK build/x86_64/KoreanSetup.o -lbe

ls -l "$OUT"
