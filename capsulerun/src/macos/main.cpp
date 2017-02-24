
#include "capsulerun.h"

#include "../shared/env.h" // merge_envs
#include "../shared/io.h" // create_fifo, receive stuff

#include <stdio.h>
#include <stdlib.h>

// posix_spawn
#include <spawn.h>

// unlink
#include <unistd.h>

// strerror
#include <string.h>

#include <thread>

using namespace std;

static char *mapped;

int receive_video_format (encoder_private_t *p, video_format_t *vfmt) {
  return capsule_receive_video_format(p->fifo_r_file, vfmt, &mapped);
}

int receive_video_frame (encoder_private_t *p, uint8_t *buffer, size_t buffer_size, int64_t *timestamp) {
  return capsule_receive_video_frame(p->fifo_r_file, buffer, buffer_size, timestamp, mapped);
}

int capsulerun_main (int argc, char **argv) {
  capsule_log("thanks for flying capsule on macOS");

  if (argc < 3) {
    capsule_log("usage: capsulerun LIBCAPSULE_DIR EXECUTABLE");
    exit(1);
  }

  char *libcapsule_dir = argv[1];
  char *executable_path = argv[2];

  string libcapsule_path = string(libcapsule_dir) + "/libcapsule.dylib"

  // TODO: respect outside DYLD_INSERT_LIBRARIES ?
  auto dyld_insert_var = "DYLD_INSERT_LIBRARIES=" + libcapsule_path;

  // swapped on purpose
  auto fifo_r_var = "CAPSULE_PIPE_R_PATH=" + string(fifo_w_path);
  auto fifo_w_var = "CAPSULE_PIPE_W_PATH=" + string(fifo_r_path);
  char *env_additions[] = {
    (char *) fifo_r_var.c_str(),
    (char *) fifo_w_var.c_str(),
    (char *) dyld_insert_var.c_str(),
    NULL
  }

  char **child_environ = merge_envs(environ, env_additions);

  pid_t child_pid;
  char **child_argv = &argv[2];

  // remove previous fifo if any  
  unlink(fifo_path);

  // create fifo
  int fifo_ret = mkfifo(fifo_path, 0644);
  if (fifo_ret != 0) {
    capsule_log("could not create fifo (code %d)", fifo_ret);
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
    capsule_log("spawn error %d: %s", err, strerror(err));
  }

  capsule_log("pid %d given to child %s", child_pid, executable_path);

  FILE *fifo_r_file = fopen(fifo_r_path.c_str(), "rb");
  if (fifo_r_file == NULL) {
    capsule_log("could not open fifo for reading: %s", strerror(errno));
    exit(1);
  }
  capsule_log("opened fifo for reading");

  FILE *fifo_w_file = fopen(fifo_w_path.c_str(), "wb");
  if (fifo_file == NULL) {
    capsule_log("could not open fifo for writing: %s", strerror(errno));
    exit(1);
  }
  capsule_log("opened fifo for writing");

  struct encoder_private_s private_data;
  memset(&private_data, 0, sizeof(private_data));
  private_data.fifo_r_file = fifo_r_file;
  private_data.fifo_w_file = fifo_w_file;

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
      capsule_log("could not wait on child (error %d): %s", wait_result, strerror(wait_result));
      exit(1);
    }

    if (WIFEXITED(child_status)) {
      capsule_log("exited, status=%d", WEXITSTATUS(child_status));
    } else if (WIFSIGNALED(child_status)) {
      capsule_log("killed by signal %d", WTERMSIG(child_status));
    } else if (WIFSTOPPED(child_status)) {
      capsule_log("stopped by signal %d", WSTOPSIG(child_status));
    } else if (WIFCONTINUED(child_status)) {
      capsule_log("continued");
    }
  } while (!WIFEXITED(child_status) && !WIFSIGNALED(child_status));

  capsule_log("waiting for encoder thread...");
  encoder_thread.join();

  return 0;
}
