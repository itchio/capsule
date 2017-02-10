
#include <stdio.h>
#include <stdlib.h>

// environ
#include <unistd.h>

// posix_spawn
#include <spawn.h>

// waitpid
#include <sys/types.h>
#include <sys/wait.h>

// mkfifo
#include <sys/stat.h>

// strerror
#include <string.h>

// errno
#include <errno.h>

// thread
#include <thread>

#include "capsulerun.h"

using namespace std;

typedef struct encoder_private_s {
  FILE *fifo_file;
} encoder_private_t;

int receive_video_resolution (encoder_private_t *p, int64_t *width, int64_t *height) {
  // FIXME: error checking
  fread(width, sizeof(int64_t), 1, p->fifo_file);
  fread(height, sizeof(int64_t), 1, p->fifo_file);
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
  printf("thanks for flying capsule on GNU/Linux\n");

  if (argc < 3) {
    printf("usage: capsulerun LIBCAPSULE_DIR EXECUTABLE\n");
    exit(1);
  }

  char *libcapsule_dir = argv[1];
  char *executable_path = argv[2];

  char libcapsule_path[CAPSULE_MAX_PATH_LENGTH];
  const int libcapsule_path_length = snprintf(libcapsule_path, CAPSULE_MAX_PATH_LENGTH, "%s/libcapsule.so", libcapsule_dir);

  if (libcapsule_path_length > CAPSULE_MAX_PATH_LENGTH) {
    printf("libcapsule path too long (%d > %d)\n", libcapsule_path_length, CAPSULE_MAX_PATH_LENGTH);
    exit(1);
  }

  pid_t child_pid;
  char **child_argv = &argv[2];

  if (setenv("LD_PRELOAD", libcapsule_path, 1 /* replace */) != 0) {
    printf("couldn't set LD_PRELOAD'\n");
    exit(1);
  }

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

  // spawn game
  int child_err = posix_spawn(
    &child_pid,
    executable_path,
    NULL, // file_actions
    NULL, // spawn_attrs
    child_argv,
    environ // environment
  );
  if (child_err != 0) {
    printf("child spawn error %d: %s\n", child_err, strerror(child_err));
  }

  printf("pid %d given to child %s\n", child_pid, executable_path);

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
  encoder_params.receive_video_resolution = (receive_video_resolution_t) receive_video_resolution;
  encoder_params.receive_video_frame = (receive_video_frame_t) receive_video_frame;

  thread encoder_thread(encoder_run, &encoder_params);

  int child_status;
  pid_t wait_result;

  do {
    wait_result = waitpid(child_pid, &child_status, 0);
    if (wait_result == -1) {
      printf("could not wait on child (error %d): %s\n", wait_result, strerror(wait_result));
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
