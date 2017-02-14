# capsule

capsule lets players easily capture short videos of games so that they can
be shared later on - see this tweet for example:

[![](https://cloud.githubusercontent.com/assets/7998310/22940649/fd0f4806-f2e2-11e6-9467-80426e12fd7a.png)](https://cloud.githubusercontent.com/assets/7998310/22940649/fd0f4806-f2e2-11e6-9467-80426e12fd7a.png)

It's being developed mainly for integration with the [itch.io app](https://itch.io/app),
but is designed as a separate component.

## Design goals

  * Easy to use - no configuration, hit a key = record (think FRAPS)
  * Fast - should slow down games as little as possible
  * Quality recordings - high FPS, preserve colorspace when possible
  * Desktop audio - all captured gameplay should come with audio
  * Easy sharing - output should be immediately useful (H264/AAC)

## Non-goals

Things that are *out of scope* (ie. that capsule will not officially support):

  * Compositing (more than one video source, etc.)
  * Live-streaming
  * Any platforms that aren't Linux, macOS, or Windows (especially mobile, BSDs, etc.)
  * Long videos - haven't settled on a maximum yet but "a few minutes"
  * Hooking into existing processes - capsulerun must be used to launch game

## General idea

Games are launched by `capsulerun`, which injects `libcapsule` into them.

`libcapsule` is responsible for detecting which Video & Audio APIs are used, and intercepting
calls to them so that it may capture video and audio frames.

`capsulerun` is responsible for receiving video and audio frames, encoding, and muxing them.

## Status

### Shared

  * Video
    * [ ] OpenGL capture
      * [x] Naive (synchronous glReadPixels)
      * [ ] Async download
      * [ ] GPU scale/colorspace conversion
      * [ ] Overlay
    * [ ] Encoding
      * [x] H264 veryfast encode at variable fps
      * [x] AAC sample conversion + encoding
      * [x] Muxing into MP4
      * [ ] Encoder-side frame buffer / async frame download

### Linux

  * System
    * [x] capsulerun launches child
    * [x] capsulerun passes arguments
    * [ ] capsulerun handles child quitting unexpectedly
    * [ ] capsulerun relays stdout/stderr
    * [ ] capsulerun relays exit code
    * [x] Interposing via `LD_PRELOAD` dlopen/dlsym
      * [ ] capsulerun picks correct library architecture to inject
    * [x] Interposing dlopen/dlsym
    * [x] Communicate by fifo
    * [ ] Shared memory
  * Control
    * [x] X11 recording hotkey support
  * Video
    * [ ] X11 capture
    * [ ] Vulkan capture
      * Basic research needed
    * [ ] Wayland capture
      * Basic research needed
  * Audio
    * PulseAudio capture
      * [x] Naive (blocking libpasimple calls)
    * ALSA capture
      * oh no.
    * Every other audio API
      * Unclear if within scope yet

### macOS

  * System
    * [x] capsulerun launches child
    * [x] capsulerun passes arguments
    * [ ] capsulerun preserves environment
      * (`DYLD_INSERT_LIBRARIES` is clobbered)
    * [ ] capsulerun handles child quitting unexpectedly
    * [ ] capsulerun relays stdout/stderr
    * [ ] capsulerun relays exit code
    * [x] Interposing via `DYLD_INSERT_LIBRARIES`
      * [ ] capsulerun picks correct library architecture to inject
    * [x] Interposing via Objective-C method swizzling
    * [x] Communicate by fifo
    * [ ] Shared memory
  * Control
    * [x] Cocoa recording hotkey support
  * Video
    * [ ] Cocoa capture
      * Basic research done, not usable yet
  * CoreAudio capture
    * [x] Creates virtual audio device for game, plays through to main output device

### Windows

  * System
    * [x] capsulerun launches child
    * [ ] capsulerun passes arguments
    * [x] capsulerun preserves environment
    * [ ] capsulerun handles child quitting unexpectedly
    * [ ] capsulerun relays stdout/stderr
    * [ ] capsulerun relays exit code
    * [x] Hooks various libraries via Deviare-InProc
      * [ ] capsulerun picks correct library architecture to inject
    * [x] Communicate by named pipe
    * [ ] Shared memory
  * Control
    * [ ] Win32 recording hotkey support
      * (but basic research done)
  * Video
    * [ ] Direct3D 8 capture
    * [ ] Direct3D 9 capture
    * [ ] Direct3D 10 capture
    * [ ] Direct3D 11 capture
      * [x] Hooks DXGI factory
      * [ ] Hooks present
      * [ ] Retrieves backbuffer
    * [ ] Direct3D 12 capture
    * [ ] Vulkan capture
  * CoreAudio capture
    * [x] Creates virtual audio device for game, plays through to main output device

## Build instructions

CMake lets us use gcc on Linux, clang on macOS and MSVC on Windows.

Always build out-of-tree (make a build dir, run cmake in there, etc. - don't
clobber the source tree).

### Building on Linux

Notes: some distributions ship libav, most distributions ship outdated
versions of ffmpeg, save yourself some trouble and build x264 and ffmpeg 3.2+
yourself into a prefix. You can then pass it to cmake with `-DCAPSULE_FFMPEG_PREFIX=/path/to/`

64-bit and 32-bit versions are built separately:

```bash
mkdir build64
cd build64
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j4
```

Capsulerun usage:

```bash
capsulerun/capsulerun $PWD/libcapsule path/to/some/game.x86_64
```

When launching, one might want to override mesa version:

```bash
MESA_GL_VERSION_OVERRIDE=2.0
MESA_GLSL_VERSION_OVERRIDE=150
```

### Building on macOS

Pretty much the same as Linux.

Note: homebrew-installed versions of ffmpeg & x264 are *not* universal, so
you might need `-DOSX_UNIVERSAL=OFF` and do 64-bit only.

capsulerun doesn't grok app bundles, so launch the exec directly: macOS is smart
enough to do the right thing.

### Building on Windows

MSVC is needed, 2015 is what we're using internally.

CMake shipped with msys2 won't cut it, you need a Windows release of CMake (which
includes the Visual Studio project generators): ,https://cmake.org/>

```bash
mkdir build64
cd build64
/c/Program\ Files/CMake/bin/cmake.exe -G "Visual Studio 14 2015 Win64" ..
```

Then, in `Visual C++ 2015 Native Build Tools Command Prompt`

```batch
cd C:\path\to\build64
msbuild ALL_BUILD.vxcproj
```

(Drop the ` Win64` to build for 32-bit)

Note: on Windows, binaries end up in `Debug` or `Release` subfolders (pass `/p:Configuration=Release` to
msbuild to pick one). `-DCMAKE_BUILD_TYPE` is ignored, so don't bother.

capsulerun usage:

```
C:\path\to\build64\capsulerun\Debug\capsulerun.exe C:\path\to\build64\libcapsule\Debug some_game.exe
```

Note that at the time of this writing (git blame is your friend), capsulerun doesn't yet relay arguments to the child.

## License

capsule is released under the GPL v2 license, see the `LICENSE` file.

It uses the following libraries:

  * ffmpeg (for AAC encoding, muxing): <https://ffmpeg.org/>
  * x264 (for H264 encoding): <http://www.videolan.org/developers/x264.html>
  * Deviare-InProc (for hooking on Windows): <https://github.com/nektra/Deviare-InProc/>
  * OpenGL, DXGI, Direct3D for capture

Some of its code is derived from the following projects:

  * OBS studio: <https://github.com/jp9000/obs-studio>
  * CoreAudio samples from Apple (for playthrough)

Special thanks to:

  * [@moonscript](https://twitter/moonscript) for sponsoring my work on it as part of <https://itch.io>
  * [@Polyflare](https://twitter.com/Polyflare) for advice and code
  * [@roxlu](https://twitter.com/roxlu) for advice and code
