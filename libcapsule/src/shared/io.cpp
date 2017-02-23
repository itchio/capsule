
#include <capsule.h>

static FILE *outFile;

int ensure_outfile() {
  if (!outFile) {
#if defined(CAPSULE_WINDOWS)
    const int pipe_path_len = CAPSULE_LOG_PATH_SIZE;
    wchar_t *pipe_path = (wchar_t*) malloc(sizeof(wchar_t) * pipe_path_len);
    pipe_path[0] = '\0';
    GetEnvironmentVariable(L"CAPSULE_PIPE_PATH", pipe_path, pipe_path_len);
    assert("Got pipe path", wcslen(pipe_path) > 0);

    capsule_log("Pipe path: %S", pipe_path);

    outFile = _wfopen(pipe_path, L"wb");
    free(pipe_path);
#else
#if defined(CAPSULE_LINUX)
    char *pipe_path = getenv("CAPSULE_PIPE_W_PATH");
#else
    char *pipe_path = getenv("CAPSULE_PIPE_PATH");
#endif
    capsule_log("Pipe path: %s", pipe_path);
    outFile = fopen(pipe_path, "wb");
#endif
    assert("Opened output file", !!outFile);
    return 1;
  }

  return 0;
}

void CAPSULE_STDCALL capsule_write_video_format (int width, int height, int format, int vflip, int pitch) {
  ensure_outfile();
  int64_t num = (int64_t) width;
  fwrite(&num, sizeof(int64_t), 1, outFile);

  num = (int64_t) height;
  fwrite(&num, sizeof(int64_t), 1, outFile);

  num = (int64_t) format;
  fwrite(&num, sizeof(int64_t), 1, outFile);

  num = (int64_t) vflip;
  fwrite(&num, sizeof(int64_t), 1, outFile);

  num = (int64_t) pitch;
  fwrite(&num, sizeof(int64_t), 1, outFile);
}

void CAPSULE_STDCALL capsule_write_video_frame (int64_t timestamp, char *frameData, size_t frameDataSize) {
  ensure_outfile();
  fwrite(&timestamp, 1, sizeof(int64_t), outFile);
  fwrite(frameData, 1, frameDataSize, outFile);
}
