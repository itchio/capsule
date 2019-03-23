# capsule

Capsule augments any game so that pressing a KEY, records a video+audio clip. Those clips
can be easily shared later, see [this tweet](https://twitter.com/moonscript/status/846061609009143809) for example.

It's being developed mainly for integration with the [itch.io app](https://itch.io/app),
but is designed as a separate pair of projects:

`libcapsule` is responsible for detecting which video & audio APIs a game uses, and intercepting
calls to them so that it may capture video and audio frames.

`capsulerun` is responsible for launching the game, injecting libcapsule, establishing a connection to it,
receiving video and audio frames, encoding, and muxing them.

See [Build instructions](#build-instructions) if you know enough to give it a shot :)

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

## Status

### Core

  * System
    * [x] capsulerun launches child, relays stdout/stderr, injects 32-bit & 64-bit processes properly
    * [x] capsulerun handles child quitting unexpectedly, relays exit code
    * [x] capsulerun & libcapsule communicate via a pair of fifos/named pipes, send frames via shared memory
    * [x] capsulerun has 'headless' mode where it doesn't launch a child, acts as just an encoder (HTML5 games, etc.)
  * Video
    * [x] encoder outputs variable fps h264 video, aac audio, in an mp4 container

### Linux

  * Control
    * [x] X11 recording hotkey support (hardcoded to F9)
  * Video
    * [x] OpenGL capture
    * [ ] Vulkan capture
  * Audio
    * [x] ALSA: Intercepts ALSA API calls, only F32LE supported so far
    * [x] PulseAudio: Captures monitor of default output device

### macOS

  * Control
    * [x] Cocoa recording hotkey support (hardcoded to F9)
  * Video
    * [x] OpenGL capture
    * [ ] Vulkan capture
  * Audio
    * [x] CoreAudio: Intercepts API calls, only F32LE supported so far

### Windows

  * Control
    * [x] Win32 recording hotkey support (hardcoded to F9)
  * Video
    * [x] Direct3D 9 capture
    * [ ] Direct3D 10 capture
    * [x] Direct3D 11 capture (with GPU color conversion & dummy overlay)
    * [ ] Direct3D 12 capture
    * [ ] Vulkan capture
  * Audio
    * [x] Wasapi: Intercepts API calls, only F32LE supported so far
    * [x] Wasapi fallback: captures default loopback device

## Build instructions

**This project is being ported to Go & Rust. These instructions are OUTDATED.**

capsule is a (set of) CMake projects, which handles things like:

  * Download some dependencies
  * Generating Makefiles, MSVC project files, etc.
  * Doing 32/64-bit tests
  * Bundling required libraries with the final executable

Like typical CMake projects, capsule should be built in a subfolder, never 
directly in the source tree, as you'll see in platform-specific instructions

### Building on Linux

Notes: some distributions ship libav, most distributions ship outdated
versions of ffmpeg, save yourself some trouble and build x264 and ffmpeg 3.2+
yourself into a prefix. You can then pass it to cmake with `-DCAPSULE_DEPS_PREFIX=/path/to/prefix`

64-bit and 32-bit versions are built separately. Example of 64-bit build:

```bash
cd /path/to/capsule
mkdir build64
cd build64
cmake ..
make install -j4
```

capsulerun usage:

```bash
/path/to/capsule/build64/dist/capsulerun -- path/to/some/game.x86_64
```

If nothing is happening, the game isn't using OpenGL, you've got the wrong capsule architecture
(64-bit for a 32-bit game for example), or there's something capsule doesn't support yet.

NB: Launcher scripts may fail and give you a bit of trouble at the moment, as capsulerun will establish
a connection with the shell and not the game. This will be addressed later.

### Building on macOS

Pretty much the same as Linux, except you don't need to compile x264 & ffmpeg yourself,
a stripped-down home-cooked universal build will be downloaded by cmake.

The resulting capsulerun & libcapsule are universal, which means there's no separate
32-bit/64-bit build processes and they should work with every app, no matter what architecture.

Example build:

```bash
cd /path/to/capsule
mkdir build
cd build
cmake ..
make install -j4
```

capsulerun supports launching .app bundles and invididual executables. Example usage:

```bash
/path/to/capsule/build/dist/capsulerun -- path/to/some/Game.app
```

### Building on Windows

Important notes:

  * Microsoft Visual C++ (MSVC) is needed, 2015 is what we're using internally.
  * CMake shipped with msys2 won't cut it, you need a Windows release of CMake (which
  includes the Visual Studio project generators): <https://cmake.org/>

**You probably won't get anywhere with an MSVC older than 2015.**

From the `Visual C++ 2015 Native Build Tools Command Prompt`, run:

```batch
cd C:\path\to\capsule
mkdir build32
cd build32
cmake -G "Visual Studio 14 2015" ..
```
To compile, run this from the build32 folder:

```batch
msbuild INSTALL.vcxproj
```

(You can use `/maxcpucount:n` to speed up the build process, since capsule is
broken down in a few projects - where `n` is the maximum number of CPUs you want to use)

To build a 64-bit version, add ` Win64` to the generator name (the `-G` parameter).

```
C:\path\to\capsule\build32\dist\capsulerun.exe -- some_game.exe
```

NB: Some games with launchers (for example UE4 games) may give you a bit of trouble at the moment,
as capsulerun will establish a connection with the launcher and not the game, which will fail.
This will be addressed later.

## License

capsule is released under the GPL v2 license, see the `LICENSE` file.

It uses the following libraries:

  * ffmpeg (for AAC encoding, muxing): <https://ffmpeg.org/>
  * x264 (for H264 encoding): <http://www.videolan.org/developers/x264.html>
  * Deviare-InProc (for hooking on Windows): <https://github.com/nektra/Deviare-InProc/>
  * argparse: <https://github.com/cofyc/argparse>
  * OpenGL, DXGI, Direct3D for capture

Some of its code is derived from the following projects:

  * OBS studio: <https://github.com/jp9000/obs-studio>

Special thanks to:

  * [@moonscript](https://twitter/moonscript) for sponsoring my work on it as part of <https://itch.io>
  * [@Polyflare](https://twitter.com/Polyflare) for advice and code
  * [@roxlu](https://twitter.com/roxlu) for advice and code

