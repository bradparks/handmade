#!/bin/sh

handmade_optimized_src=`pwd`/code/handmade_optimized.cpp
handmade_src=`pwd`/code/handmade.cpp
handmade_platform_src=`pwd`/code/sdl_handmade.cpp
cflags="-g -Wall -W -Wno-writable-strings -Wno-unused-value -Wno-missing-braces -Wno-unused-function -Wno-unused-variable -Wno-unused-parameter -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1 -DHANDMADE_SDL=1 -DCOMPILER_LLVM -std=c++11"
ldflags="-lsdl2"

[ ! -d "build" ] && mkdir build
pushd build

clang $cflags -O2 $handmade_optimized_src -c -o handmade_optimized.o

clang $cflags $ldflags -dynamiclib $handmade_src handmade_optimized.o -o handmade.dylib

clang $cflags $ldflags $handmade_platform_src -o sdl_handmade

popd
