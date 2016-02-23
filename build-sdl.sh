#!/bin/sh

handmade_src=`pwd`/code/handmade.cpp
handmade_platform_src=`pwd`/code/sdl_handmade.cpp
cflags="-W -Wno-unused-parameter -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1 -DHANDMADE_SDL=1 -std=c++11"
ldflags="-lsdl2"

[ ! -d "build" ] && mkdir build
pushd build

clang $cflags -dynamiclib $handmade_src -o handmade.dylib

clang $cflags $ldflags $handmade_platform_src -o sdl_handmade

popd
