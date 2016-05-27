#!/bin/sh

CommonCompilerFlags="-Od -MTd -nologo -fp:fast -Gm- -GR- -EHa- -Oi -WX -W4 -wd4201 -wd4100 -wd4189 -wd4505 -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1 -DHANDMADE_WIN32=1 -FC -Z7"
CommonLinkerFlags="-incremental:no -opt:ref user32.lib gdi32.lib winmm.lib"

[ ! -d "build" ] && mkdir build
pushd build

rm -f *.pdb
echo WAITING FOR PDB > lock.tmp

# Asset file builder build
cl $CommonCompilerFlags -DTRANSLATION_UNIT_INDEX=0 -D_CRT_SECURE_NO_WARNINGS ../code/test_asset_builder.cpp /link $CommonLinkerFlags

cl $CommonCompilerFlags -DTRANSLATION_UNIT_INDEX=1 -O2 -c ../code/handmade_optimized.cpp -Fohandmade_optimized.obj -LD
cl $CommonCompilerFlags -DTRANSLATION_UNIT_INDEX=0 ../code/handmade.cpp handmade_optimized.obj -Fmhandmade.map -LD /link -incremental:no -opt:ref -PDB:handmade_$RANDOM.pdb -EXPORT:GameGetSoundSamples -EXPORT:GameUpdateAndRender -EXPORT:DEBUGGameFrameEnd

rm lock.tmp
cl $CommonCompilerFlags -DTRANSLATION_UNIT_INDEX=2 ../code/win32_handmade.cpp -Fmwin32_handmade.map /link $CommonLinkerFlags

popd
