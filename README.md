# capsule

## License

capsule is released under the MIT license, see the `LICENSE` file.

## Linux

```bash
mkdir build64
cd build64
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j4
cd ..
capsulerun/capsulerun $PWD/libcapsule path/to/some/game.x86_64
```

Might want to override mesa version:

```bash
MESA_GL_VERSION_OVERRIDE=2.0
MESA_GLSL_VERSION_OVERRIDE=150
```

## Mac

Same as linux but without the MESA mess

## Windows

TODO: allow building win32 or win64 with same CMakeLists, for now, using `deps32/` and `deps64/` dirs, with:

  * Hand-built glew (from <https://github.com/Perlmint/glew-cmake>)
  * MSVC SDL2 (from <https://www.libsdl.org/download-2.0.php>)

TODO: upload builds of those somewhere accessible.

When preparing `deps32/` make sure to remove `x64` directory

```
mkdir build64
cd build64
/c/Program\ Files/CMake/bin/cmake.exe -G "Visual Studio 14 2015 Win64" ..
msbuild ALL_BUILD.vxcproj
```

(It looks like the generator name changed in CMake 3.7)

(Drop the ` Win64` to build for 32-bit)

Copy (the right) SDL2.dll by hand next to main.exe before launching.

CWD matters (if Debugging from MSVC, change main's Properties -> Debugging ->
Working Directory, set to `..`).

Re detours:

  * taksi approach worked on win32, broke on win64
  * MologieDetours worked on win32, broke on win64
  * Deviare-InProc worked on both but was GPL

