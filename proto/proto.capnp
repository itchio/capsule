@0xaebd06712ae3dc6d;

interface Host {
  registerTarget @0 (target: Target);
  # Called when we first detect OpenGL/Direct3D usage 

  notifyFrame @1 ();
  # Just testing

  interface Target {
    struct Info {
      pid @0 :UInt64;
      # PID of the target process

      exe @1 :Text;
      # Executable path of the target process
    }

    getInfo @0 () -> (info :Info);
    # Returns information about the target

    startCapture @1 (sink :Sink) -> (session :Session);
    # Ask the target to start capturing into sink.
  }

  interface Session {
    struct Info {
      audio @0 :Audio;
      video @1 :Video;

      struct Video {
        width @0 :UInt32;
        # 1920, etc.
        height @1 :UInt32;
        # 1080, etc.
      }

      struct Audio {
        sampleRate @0 :UInt32;
        # 44100 Hz, etc.
      }
    }

    info @0 () -> (info :Info);
    # Get info about the session

    stop @1 ();
    # Stop capturing this session
  }

  interface Sink {
    sendVideoFrame @0 (frame: VideoFrame);
  }

  struct VideoFrame {
    timestamp @0 :Timestamp;
    index @1 :UInt32;
  }

  struct Timestamp {
    time @0 :Float64;
    # Number of milliseconds since start of the session
    # This might be a terrible way to measure time.
  }
}
