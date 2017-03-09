rem You need to have run cmake by yourself first

pushd build32
msbuild ALL_BUILD.vcxproj
popd

pushd build64
msbuild ALL_BUILD.vcxproj
popd

mkdir build
xcopy /y /i build64\capsulerun\Debug\*.exe build\
xcopy /y /i build64\capsulerun\Debug\*.dll build\
copy /y build32\libcapsule\Debug\capsule.dll build\capsule32.dll
copy /y build64\libcapsule\Debug\capsule.dll build\capsule64.dll
copy /y res\*.png build\
