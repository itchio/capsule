rem You need to have run cmake by yourself first

pushd build32
msbuild INSTALL.vcxproj
popd

pushd build64
msbuild INSTALL.vcxproj
popd

mkdir build
xcopy /y /i build64\dist\* build\
xcopy /y /i build32\dist\capsule32.dll build\
