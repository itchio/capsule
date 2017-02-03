
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


#include "capsulerun.h"

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

  // char *env[2];
  // // TODO: respect outside LD_PRELOAD ?
  // env[0] = so_preload;
  // env[1] = NULL;

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

  const char *ffmpeg_path = "/usr/bin/ffmpeg";
  char *ffmpeg_argv[26];
  ffmpeg_argv[0] = (char*) ffmpeg_path;
  ffmpeg_argv[1] = (char*) "-y";
  ffmpeg_argv[2] = (char*) "-f";
  ffmpeg_argv[3] = (char*) "rawvideo";
  ffmpeg_argv[4] = (char*) "-framerate";
  ffmpeg_argv[5] = (char*) "30";
  ffmpeg_argv[6] = (char*) "-pix_fmt";
  ffmpeg_argv[7] = (char*) "rgb24";
  ffmpeg_argv[8] = (char*) "-s";
  ffmpeg_argv[9] = (char*) "800x600";
  ffmpeg_argv[10] = (char*) "-i";
  ffmpeg_argv[11] = fifo_path;
  ffmpeg_argv[12] = (char*) "-c:v";
  ffmpeg_argv[13] = (char*) "libx264";
  ffmpeg_argv[14] = (char*) "-preset";
  ffmpeg_argv[15] = (char*) "ultrafast";
  ffmpeg_argv[16] = (char*) "-crf";
  ffmpeg_argv[17] = (char*) "23";
  ffmpeg_argv[18] = (char*) "-r";
  ffmpeg_argv[19] = (char*) "30";
  ffmpeg_argv[20] = (char*) "-pix_fmt";
  ffmpeg_argv[21] = (char*) "yuv420p";
  ffmpeg_argv[22] = (char*) "-vf";
  ffmpeg_argv[23] = (char*) "vflip";
  ffmpeg_argv[24] = (char*) "movie.mp4";
  ffmpeg_argv[25] = NULL;

  pid_t ffmpeg_pid;

  posix_spawn_file_actions_t ffmpeg_actions;
  int fa_ret;

  if (fa_ret = posix_spawn_file_actions_init (&ffmpeg_actions)) {
    printf("posix_spawn_file_actions_init\n");
    exit(fa_ret);
  }

  if (fa_ret = posix_spawn_file_actions_adddup2 (&ffmpeg_actions, 1, 1)) {
    printf("posix_spawn_file_actions_adddup2 (1)\n");
    exit(fa_ret);
  }

  if (fa_ret = posix_spawn_file_actions_adddup2 (&ffmpeg_actions, 2, 2)) {
    printf("posix_spawn_file_actions_adddup2 (2)\n");
    exit(fa_ret);
  }

  // spawn ffmpeg
  int ffmpeg_err = posix_spawn(
    &ffmpeg_pid,
    ffmpeg_path,
    &ffmpeg_actions, // file_actions
    NULL, // spawn_attrs
    ffmpeg_argv,
    environ // environment
  );
  if (ffmpeg_err != 0) {
    printf("ffmpeg spawn error %d: %s\n", ffmpeg_err, strerror(ffmpeg_err));
  }

  printf("spawned ffmpeg (pid %d)\n", ffmpeg_pid);

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

  return 0;
}
