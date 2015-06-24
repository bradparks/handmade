#!/bin/sh

mkdir ../build
pushd ../build
gcc -g ../code/win32_handmade.cpp -luser32
popd
