SET cmake = C:\Program Files\CMake\bin\cmake.exe

mkdir build32
pushd build32
"%cmake%" -G "Visual Studio 14 2015" ..
popd

mkdir build64
pushd build64
"%cmake%" -G "Visual Studio 14 2015 Win64" ..
popd