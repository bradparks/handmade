#!/bin/sh

mkdir ../build
pushd ../build
gcc -Wall -Wno-unused-variable -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1 -DHANDMADE_WIN32=1 -g ../code/win32_handmade.cpp -luser32 -lgdi32
popd
