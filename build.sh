#!/bin/bash

cd $(dirname $0)

if [[ $OSTYPE == "darwin"* ]]; then
    source ./build-sdl.sh
elif [ $OSTYPE == "win32" ] || [ $OSTYPE == "cygwin" ]; then
    source ./build-win32.sh
else
    echo "UNSUPOORT PLATFORM $OSTYPE"
    exit
fi
