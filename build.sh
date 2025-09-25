#!/bin/sh -e
cd "${0%/*}"

code=$PWD/code
build=$PWD/build
mkdir -p "$build"
cd "$build"

compiler_flags="-O0 -g -Werror -Wall -Wextra -Wshadow -Wconversion -Wno-unused-function -Wno-unused-parameter -Wno-unused-variable -Wno-unused-but-set-variable -Wno-string-conversion"
compiler_flags="$compiler_flags $(pkgconf --cflags --libs dbus-1)"

echo Starting build.
clang $compiler_flags "$code"/trayge.c -o trayge