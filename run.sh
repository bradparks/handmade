#!/bin/sh

if [[ $OSTYPE == "darwin"* ]]; then
    exe="sdl_handmade"
elif [ $OSTYPE == "win32" ] || [ $OSTYPE == "cygwin" ]; then
    exe="win32_handmade.exe"
else
    echo "UNSUPOORT PLATFORM $OSTYPE"
    exit
fi

cd data/ && ../build/$exe
