#/bin/sh -xe

(cd build32/libcapsule && make -j4)
(cd build64 && make -j4)

mkdir -p build
cp -rf build64/capsulerun/capsulerun build/
cp -rf build32/libcapsule/libcapsule.so build/libcapsule32.so
cp -rf build64/libcapsule/libcapsule.so build/libcapsule64.so
