#!/bin/sh

CommonCompilerFlags="-O2 -MTd -nologo -fp:fast -Gm- -GR- -EHa- -Oi -WX -W4 -wd4201 -wd4100 -wd4189 -wd4505 -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1 -DHANDMADE_WIN32=1 -FC -Z7"
CommonLinkerFlags="-incremental:no -opt:ref user32.lib gdi32.lib winmm.lib"

[ ! -d "build" ] && mkdir build
pushd build

rm -f *.pdb
echo WAITING FOR PDB > lock.tmp
cl $CommonCompilerFlags ../code/handmade.cpp -Fmhandmade.map -LD /link -incremental:no -opt:ref -PDB:handmade_$RANDOM.pdb -EXPORT:GameGetSoundSamples -EXPORT:GameUpdateAndRender

rm lock.tmp
cl $CommonCompilerFlags ../code/win32_handmade.cpp -Fmwin32_handmade.map /link $CommonLinkerFlags

popd
