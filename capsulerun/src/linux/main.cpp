
#include "capsulerun.h"

#include "../shared/env.h" // merge_envs
#include "../shared/io.h" // create_fifo, receive stuff

#include <stdio.h>
#include <stdlib.h>

// environ
#include <unistd.h>

// posix_spawn
#include <spawn.h>

// waitpid
#include <sys/types.h>
#include <sys/wait.h>

// strerror
#include <string.h>

// errno
#include <errno.h>

// thread
#include <thread>

using namespace std;

int receive_video_format (encoder_private_t *p, video_format_t *vfmt) {
  return capsule_io_receive_video_format(p->io, vfmt);
}

int receive_video_frame (encoder_private_t *p, uint8_t *buffer, size_t buffer_size, int64_t *timestamp) {
  return capsule_io_receive_video_frame(p->io, buffer, buffer_size, timestamp);
}

int capsulerun_main (int argc, char **argv) {
  capsule_log("thanks for flying capsule on GNU/Linux");

  if (argc < 3) {
    capsule_log("usage: capsulerun LIBCAPSULE_DIR EXECUTABLE");
    exit(1);
  }

  char *libcapsule_dir = argv[1];
  char *executable_path = argv[2];

  char libcapsule_path[CAPSULE_MAX_PATH_LENGTH];
  const int libcapsule_path_length = snprintf(libcapsule_path, CAPSULE_MAX_PATH_LENGTH, "%s/libcapsule.so", libcapsule_dir);

  if (libcapsule_path_length > CAPSULE_MAX_PATH_LENGTH) {
    capsule_log("libcapsule path too long (%d > %d)", libcapsule_path_length, CAPSULE_MAX_PATH_LENGTH);
    exit(1);
  }

  pid_t child_pid;
  char **child_argv = &argv[2];

  if (setenv("LD_PRELOAD", libcapsule_path, 1 /* replace */) != 0) {
    capsule_log("couldn't set LD_PRELOAD'");
    exit(1);
  }

  auto fifo_r_path = string("/tmp/capsule.runr");
  auto fifo_w_path = string("/tmp/capsule.runw");

  // swapped on purpose
  auto fifo_r_var = "CAPSULE_PIPE_R_PATH=" + fifo_w_path;
  auto fifo_w_var = "CAPSULE_PIPE_W_PATH=" + fifo_r_path;
  char *env_additions[] = {
    (char *) fifo_r_var.c_str(),
    (char *) fifo_w_var.c_str(),
    NULL
  };
  char **child_environ = merge_envs(environ, env_additions);

  capsule_io_t io = {};
  capsule_io_init(&io, fifo_r_path, fifo_w_path);

  // spawn game
  int child_err = posix_spawn(
    &child_pid,
    executable_path,
    NULL, // file_actions
    NULL, // spawn_attrs
    child_argv,
    child_environ // environment
  );
  if (child_err != 0) {
    capsule_log("child spawn error %d: %s", child_err, strerror(child_err));
  }

  capsule_log("pid %d given to child %s", child_pid, executable_path);

  capsule_io_connect(&io);

  struct encoder_private_s private_data;
  memset(&private_data, 0, sizeof(private_data));
  private_data.io = &io;

  struct encoder_params_s encoder_params;
  memset(&encoder_params, 0, sizeof(encoder_params));
  encoder_params.private_data = &private_data;
  encoder_params.receive_video_format = (receive_video_format_t) receive_video_format;
  encoder_params.receive_video_frame = (receive_video_frame_t) receive_video_frame;

  encoder_params.has_audio = 1;
  encoder_params.receive_audio_format = (receive_audio_format_t) receive_audio_format;
  encoder_params.receive_audio_frames = (receive_audio_frames_t) receive_audio_frames;

  capsule_log("starting encoder thread...");
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
