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
      * [x] Async download
      * [ ] GPU scale/colorspace conversion
      * [ ] Overlay
    * [ ] Encoding
      * [x] H264 veryfast encode at variable fps
      * [x] AAC sample conversion + encoding
      * [x] Muxing into MP4
      * [x] Encoder-side frame buffer / async frame download

### Linux

  * System
    * [x] capsulerun launches child
    * [x] capsulerun passes arguments
      * [x] quoted & with unicode handling
    * [x] capsulerun handles child quitting unexpectedly
    * [x] capsulerun relays stdout/stderr
    * [x] capsulerun relays exit code
    * [x] Interposing via `LD_PRELOAD` dlopen/dlsym
      * [x] capsulerun picks correct library architecture to inject
    * [x] Interposing dlopen/dlsym
    * [x] Communicate by fifo
    * [x] Shared memory
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
      * [x] quoted & with unicode handling
    * [ ] capsulerun preserves environment
      * (`DYLD_INSERT_LIBRARIES` is clobbered)
    * [ ] capsulerun handles child quitting unexpectedly
    * [ ] capsulerun relays stdout/stderr
    * [ ] capsulerun relays exit code
    * [x] Interposing via `DYLD_INSERT_LIBRARIES`
      * [ ] capsulerun picks correct library architecture to inject
    * [x] Interposing via Objective-C method swizzling
    * [x] Communicate by fifo
    * [x] Shared memory
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
    * [x] capsulerun passes arguments
      * [x] quoted & with unicode handling
    * [x] capsulerun preserves environment
    * [x] capsulerun handles child quitting unexpectedly
    * [x] capsulerun relays stdout/stderr
    * [x] capsulerun relays exit code
    * [x] Hooks various libraries via Deviare-InProc
      * [x] capsulerun picks correct library architecture to inject
    * [x] Communicate by named pipe
    * [x] Shared memory
  * Control
    * [x] Win32 recording hotkey support
  * Video
    * [x] Direct3D 9 capture
      * [x] Hooks D3D initialization
      * [x] Hooks device creation
      * [x] Hooks all three present flavors
      * [x] Retrieves & sends backbuffer
      * [ ] Async GPU download
    * [ ] Direct3D 10 capture
    * [x] Direct3D 11 capture
      * [x] Hooks DXGI factory creation
      * [x] Hooks CreateSwapChain
      * [x] Hooks CreateSwapChainForHwnd
      * [x] Hooks present
      * [x] Retrieves & sends backbuffer
      * [x] Async GPU download
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
make install -j4
```

Capsulerun usage:

```bash
path/to/capsule/build64/dist/capsulerun -- path/to/some/game.x86_64
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

Important notes:

Microsoft Visual C++ (MSVC) is needed, 2015 is what we're using internally.

**You probably won't get anywhere with an MSVC older than 2015.**

CMake shipped with msys2 won't cut it, you need a Windows release of CMake (which
includes the Visual Studio project generators): <https://cmake.org/>

From the `Visual C++ 2015 Native Build Tools Command Prompt`, run:

```batch
mkdir build32
cd build32
cmake -G "Visual Studio 14 2015" ..
```
To compile, run this from the build32 folder:

```batch
msbuild INSTALL.vcxproj
```

To build a 64-bit version, add ` Win64` to the generator name (the `-G` parameter).

```
C:\path\to\capsule\build32\dist\capsulerun.exe -- some_game.exe
```

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
