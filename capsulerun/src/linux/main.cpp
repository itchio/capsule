
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

#include "env.h"

using namespace std;

#include <sys/mman.h>
#include <sys/stat.h> // for mode constants
#include <fcntl.h>    // for O_* constants

#include <capsule/messages_generated.h>

using namespace Capsule::Messages;

char *capsule_read_packet (FILE *file) {
    uint32_t pkt_size = 0;
    fread(&pkt_size, sizeof(pkt_size), 1, file);
    char *buffer = new char[pkt_size];

    capsule_log("reading packet, size: %d bytes", pkt_size);
    fread(buffer, pkt_size, 1, file);
    return buffer;
}

int receive_video_format (encoder_private_t *p, video_format_t *vfmt) {
  char *buf = capsule_read_packet(p->fifo_file);

  auto pkt = GetPacket(buf);
  if (pkt->message_type() != Message_VideoSetup) {
    capsule_log("expected VideoSetup message, got %s", EnumNameMessage(pkt->message_type()));
    exit(1);
  }

  auto vs = static_cast<const VideoSetup*>(pkt->message());

  vfmt->width = vs->width();
  vfmt->height = vs->height();
  vfmt->format = (capsule_pix_fmt_t) vs->pix_fmt();
  vfmt->vflip = vs->vflip();
  vfmt->pitch = vs->pitch();

  auto shmem = vs->shmem();
  capsule_log("shm path: %s", shmem->path()->str().c_str());
  capsule_log("shm size: %d", shmem->size());

  int shmem_handle = shm_open(shmem->path()->str().c_str(), O_RDONLY, 0755);
  if (!(shmem_handle > 0)) {
    capsule_log("Could not open shmem area");
    exit(1);
  }

  char *mapped = (char *) mmap(
      nullptr, // addr
      shmem->size(), // length
      PROT_READ, // prot
      MAP_SHARED, // flags
      shmem_handle, // fd
      0 // offset
  );

  if (!mapped) {
    capsule_log("Could not map shmem area");
    exit(1);
  }
  capsule_log("shm contents: %s\n", mapped);

  delete[] buf;
  return 0;
}

int receive_video_frame (encoder_private_t *p, uint8_t *buffer, size_t buffer_size, int64_t *timestamp) {
  int read_bytes = fread(timestamp, 1, sizeof(int64_t), p->fifo_file);
  if (read_bytes == 0) {
    return 0;
  }

  return fread(buffer, 1, buffer_size, p->fifo_file);
}

void create_fifo (string fifo_path) {
  // remove previous fifo if any  
  unlink(fifo_path.c_str());

  // create fifo
  int fifo_ret = mkfifo(fifo_path.c_str(), 0644);
  if (fifo_ret != 0) {
    capsule_log("could not create fifo at %s (code %d)\n", fifo_path.c_str(), fifo_ret);
    exit(1);
  }
}

int capsulerun_main (int argc, char **argv) {
  capsule_log("thanks for flying capsule on GNU/Linux\n");

  if (argc < 3) {
    capsule_log("usage: capsulerun LIBCAPSULE_DIR EXECUTABLE\n");
    exit(1);
  }

  char *libcapsule_dir = argv[1];
  char *executable_path = argv[2];

  char libcapsule_path[CAPSULE_MAX_PATH_LENGTH];
  const int libcapsule_path_length = snprintf(libcapsule_path, CAPSULE_MAX_PATH_LENGTH, "%s/libcapsule.so", libcapsule_dir);

  if (libcapsule_path_length > CAPSULE_MAX_PATH_LENGTH) {
    capsule_log("libcapsule path too long (%d > %d)\n", libcapsule_path_length, CAPSULE_MAX_PATH_LENGTH);
    exit(1);
  }

  pid_t child_pid;
  char **child_argv = &argv[2];

  if (setenv("LD_PRELOAD", libcapsule_path, 1 /* replace */) != 0) {
    capsule_log("couldn't set LD_PRELOAD'\n");
    exit(1);
  }

  auto fifo_r_path = string("/tmp/capsule.runr");
  auto fifo_w_path = string("/tmp/capsule.runw");

  create_fifo(fifo_r_path);
  create_fifo(fifo_w_path);

  auto fifo_r_var = "CAPSULE_PIPE_R_PATH=" + fifo_w_path;
  auto fifo_w_var = "CAPSULE_PIPE_W_PATH=" + fifo_r_path;
  char *env_additions[] = {
    (char *) fifo_r_var.c_str(),
    (char *) fifo_w_var.c_str(),
    NULL
  };
  char **child_environ = merge_envs(environ, env_additions);

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
    printf("child spawn error %d: %s\n", child_err, strerror(child_err));
  }

  printf("pid %d given to child %s\n", child_pid, executable_path);

  // open fifo
  FILE *fifo_file = fopen(fifo_r_path.c_str(), "rb");
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

  encoder_params.has_audio = 1;
  encoder_params.receive_audio_format = (receive_audio_format_t) receive_audio_format;
  encoder_params.receive_audio_frames = (receive_audio_frames_t) receive_audio_frames;

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
