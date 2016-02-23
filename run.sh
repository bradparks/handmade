#!/bin/sh

if [[ "$OSTYPE" == "darwin"* ]]; then
    exe="sdl_handmade"
elif [[ "$OSTYPE" == "win32" ]]; then
    exe="win32_handmade.exe"
else
    echo "UNSUPOORT PLATFORM"
    exit
fi

cd data/ && ../build/$exe
