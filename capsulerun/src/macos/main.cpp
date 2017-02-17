
#include <stdio.h>
#include <stdlib.h>

// posix_spawn
#include <spawn.h>

// unlink
#include <unistd.h>

// mkfifo
#include <sys/types.h>
#include <sys/stat.h>

// strerror
#include <string.h>

#include <thread>

#include "capsulerun.h"

using namespace std;

int receive_video_format (encoder_private_t *p, video_format_t *vfmt) {
  // FIXME: error checking
  int64_t num;
  fread(&num, sizeof(int64_t), 1, p->fifo_file); vfmt->width = (int) num;
  fread(&num, sizeof(int64_t), 1, p->fifo_file); vfmt->height = (int) num;
  fread(&num, sizeof(int64_t), 1, p->fifo_file); vfmt->format = (int) num;
  fread(&num, sizeof(int64_t), 1, p->fifo_file); vfmt->vflip = (int) num;
  fread(&num, sizeof(int64_t), 1, p->fifo_file); vfmt->pitch = (int) num;
  return 0;
}

int receive_video_frame (encoder_private_t *p, uint8_t *buffer, size_t buffer_size, int64_t *timestamp) {
  int read_bytes = fread(timestamp, 1, sizeof(int64_t), p->fifo_file);
  if (read_bytes == 0) {
    return 0;
  }

  return fread(buffer, 1, buffer_size, p->fifo_file);
}

int capsulerun_main (int argc, char **argv) {
  printf("thanks for flying capsule on macOS\n");

  if (argc < 3) {
    printf("usage: capsulerun LIBCAPSULE_DIR EXECUTABLE");
    exit(1);
  }

  char *libcapsule_dir = argv[1];
  char *executable_path = argv[2];

  char libcapsule_path[CAPSULE_MAX_PATH_LENGTH];
  const int libcapsule_path_length = snprintf(libcapsule_path, CAPSULE_MAX_PATH_LENGTH, "%s/libcapsule.dylib", libcapsule_dir);

  if (libcapsule_path_length > CAPSULE_MAX_PATH_LENGTH) {
    printf("libcapsule path too long (%d > %d)", libcapsule_path_length, CAPSULE_MAX_PATH_LENGTH);
    exit(1);
  }

  char dyld_insert[CAPSULE_MAX_PATH_LENGTH];
  const int dyld_insert_length = snprintf(dyld_insert, CAPSULE_MAX_PATH_LENGTH, "DYLD_INSERT_LIBRARIES=%s", libcapsule_path);

  if (dyld_insert_length > CAPSULE_MAX_PATH_LENGTH) {
    printf("dyld_insert path too long (%d > %d)", dyld_insert_length, CAPSULE_MAX_PATH_LENGTH);
    exit(1);
  }

  pid_t child_pid;
  char **child_argv = &argv[2];

  char *env[2];
  // TODO: respect outside DYLD_INSERT_LIBRARIES ?
  env[0] = dyld_insert;
  env[1] = NULL;

  char cwd[CAPSULE_MAX_PATH_LENGTH];
  getcwd(cwd, CAPSULE_MAX_PATH_LENGTH);
  char fifo_path[CAPSULE_MAX_PATH_LENGTH];
  const int fifo_path_length = snprintf(fifo_path, CAPSULE_MAX_PATH_LENGTH, "%s/capsule.rawvideo", cwd);

  if (fifo_path_length > CAPSULE_MAX_PATH_LENGTH) {
    printf("fifo path too long (%d > %d)\n", fifo_path_length, CAPSULE_MAX_PATH_LENGTH);
    exit(1);
  }

  // remove previous fifo if any  
  unlink(fifo_path);

  // create fifo
  int fifo_ret = mkfifo(fifo_path, 0644);
  if (fifo_ret != 0) {
    printf("could not create fifo (code %d)\n", fifo_ret);
    exit(1);
  }

  int err = posix_spawn(
    &child_pid,
    executable_path,
    NULL, // file_actions
    NULL, // spawn_attrs
    child_argv,
    env
  );
  if (err != 0) {
    printf("spawn error %d: %s", err, strerror(err));
  }

  printf("pid %d given to child %s", child_pid, executable_path);

  // open fifo
  FILE *fifo_file = fopen(fifo_path, "rb");
  if (fifo_file == NULL) {
    printf("could not open fifo for reading: %s\n", strerror(errno));
    exit(1);
  }

  printf("opened fifo\n");

  struct encoder_private_s private_data;
  memset(&private_data, 0, sizeof(private_data));
  private_data.fifo_file = fifo_file;

  struct encoder_params_s encoder_params;
  memset(&encoder_params, 0, sizeof(encoder_params));
  encoder_params.private_data = &private_data;
  encoder_params.receive_video_format = (receive_video_format_t) receive_video_format;
  encoder_params.receive_video_frame = (receive_video_frame_t) receive_video_frame;

  encoder_params.has_audio = 0;

  thread encoder_thread(encoder_run, &encoder_params);

  int child_status;
  pid_t wait_result;

  do {
    wait_result = waitpid(child_pid, &child_status, 0);
    if (wait_result == -1) {
      printf("could not wait on child (error %d): %s", wait_result, strerror(wait_result));
      exit(1);
    }

    if (WIFEXITED(child_status)) {
      printf("exited, status=%d\n", WEXITSTATUS(child_status));
    } else if (WIFSIGNALED(child_status)) {
      printf("killed by signal %d\n", WTERMSIG(child_status));
    } else if (WIFSTOPPED(child_status)) {
      printf("stopped by signal %d\n", WSTOPSIG(child_status));
    } else if (WIFCONTINUED(child_status)) {
      printf("continued\n");
    }
  } while (!WIFEXITED(child_status) && !WIFSIGNALED(child_status));

  printf("waiting for encoder thread...\n");
  encoder_thread.join();

  return 0;
}
