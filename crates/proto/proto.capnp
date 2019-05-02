@0xaebd06712ae3dc6d;

interface Host {
  registerTarget @0 (info :Target.Info, target :Target);
  # Called when we first detect OpenGL/Direct3D usage 

  hotkeyPressed @1 ();
  # Called when the hotkey is pressed

  createSink @2 (options :Sink.Options) -> (sink :Sink);
  # Called during session creation

  interface Target {
    struct Info {
      pid @0 :UInt64;
      # PID of the target process

      exe @1 :Text;
      # Executable path of the target process
    }

    startSession @0 () -> (session :Session);
    # Ask the target to start a new capture session.
  }

  interface Session {
    stop @0 ();
    # Stop this session
  }

  struct VideoFrame {
    timestamp @0 :Timestamp;
    index @1 :UInt64;
    data @2 :Data;
  }

  struct Timestamp {
    millis @0 :Float64;
    # Number of milliseconds since start of the session
    # This might be a terrible way to measure time.
  }

  interface Sink {
    struct Options {
      audio @0 :Audio;
      video @1 :Video;

      struct Video {
        width @0 :UInt32;
        # 1920, etc.
        height @1 :UInt32;
        # 1080, etc.

        pitch @2 :UInt32;
        # number of bytes between each line

        pixelFormat @3 :PixelFormat;
        # pixel format (bgra, etc.)

        verticalFlip @4 :Bool;
        # if true, the lines are Y-flipped

        enum PixelFormat {
          bgra @0;
          # 32-bit, B8G8R8A8
        }
      }

      struct Audio {
        sampleRate @0 :UInt32;
        # 44100 Hz, etc.
      }
    }

    sendVideoFrame @0 (frame: VideoFrame);
    # Send a video frame
  }
}
