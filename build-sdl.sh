#!/bin/bash

cd $(dirname $0)
base=$(pwd)

cc=clang
cflags="-g -Wall -W -Wno-writable-strings -Wno-unused-value -Wno-missing-braces -Wno-unused-function -Wno-unused-variable -Wno-unused-parameter -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1 -DHANDMADE_SDL=1 -DCOMPILER_LLVM -std=c++11"
ldflags="-lSDL2"

[ ! -d "build" ] && mkdir build

cd build

# Asset file builder build
#clang $cflags $ldflags $base/code/test_asset_builder.cpp -o test_asset_builder

clang $cflags -O2 $base/code/handmade_optimized.cpp -c -o handmade_optimized.o

clang $cflags $ldflags -dynamiclib $base/code/handmade.cpp handmade_optimized.o -o handmade.dylib

clang $cflags $ldflags $base/code/sdl_handmade.cpp -o sdl_handmade
