#include "PulseReceiver.h"

#include <chrono>

#include <string.h> // memset, memcpy

using namespace std;

static void read_loop (PulseReceiver *pr) {
  while (true) {
    if (!pr->read_from_pa()) {
      return;
    }
  }
}

PulseReceiver::PulseReceiver() {
  memset(&afmt, 0, sizeof(audio_format_t));

  if (!capsule_pulse_init()) {
    return;
  }

  /* The sample type to use */
  static const pa_sample_spec ss = {
      .format = PA_SAMPLE_FLOAT32LE, .rate = 44100, .channels = 2};

  // TODO: list devices instead
  const char *dev = "alsa_output.pci-0000_00_1b.0.analog-stereo.monitor";

  int pa_err = 0;
  ctx = _pa_simple_new(NULL,             // server
                      "capsule",        // name
                      PA_STREAM_RECORD, // direction
                      dev,              // device
                      "record",         // stream name
                      &ss,              // sample spec
                      NULL,             // channel map
                      NULL,             // buffer attributes
                      &pa_err           // error
                      );

  if (!ctx) {
    fprintf(stderr, "Could not create pulseaudio context, error %d (%x)\n",
            pa_err, pa_err);
    return;
  }

  afmt.channels = ss.channels;
  afmt.samplerate = ss.rate;
  afmt.samplewidth = 32;
  buffer_size =
      AUDIO_NB_SAMPLES * afmt.channels * (afmt.samplewidth / 8);
  in_buffer = (uint8_t *) calloc(1, buffer_size);
  buffers = (uint8_t *) calloc(AUDIO_NB_BUFFERS, buffer_size);

  for (int i = 0; i < AUDIO_NB_BUFFERS; i++) {
    buffer_state[i] = AUDIO_BUFFER_PROCESSED;
  }
  commit_index = 0;
  process_index = 0;

  // start reading
  pa_thread = new thread(read_loop, this);
}

bool PulseReceiver::read_from_pa() {
  {
    lock_guard<mutex> lock(pa_mutex);

    if (!ctx) {
      // we're closed!
      return false;
    }

    int pa_err;
    int ret = _pa_simple_read(ctx, in_buffer, buffer_size, &pa_err);
    if (ret < 0) {
        fprintf(stderr, "Could not read from pulseaudio, error %d (%x)\n", pa_err, pa_err);
        return false;
    }
  }

  // cool, so we got a buffer, now let's find room for it.
  {
    lock_guard<mutex> lock(buffer_mutex);

    if (buffer_state[commit_index] != AUDIO_BUFFER_PROCESSED) {
      // no room for it, just skip those then
      if (!overrun) {
        overrun = true;
        fprintf(stderr, "PulseReceiver: buffer overrun (captured audio but nowhere to place it)\n");
      }
      // keep trying tho
      return true;
    }

    overrun = false;

    uint8_t *target = buffers + (commit_index * buffer_size);
    memcpy(target, in_buffer, buffer_size);
    buffer_state[commit_index] = AUDIO_BUFFER_COMMITTED;
    commit_index = (commit_index + 1) % AUDIO_NB_BUFFERS;
  }

  return true;
}

int PulseReceiver::receive_format(audio_format_t *afmt_out) {
  memcpy(afmt_out, &afmt, sizeof(audio_format_t));
  return 0;
}

void *PulseReceiver::receive_frames(int *frames_received) {
  lock_guard<mutex> lock(buffer_mutex);

  if (buffer_state[process_index] != AUDIO_BUFFER_COMMITTED) {
    // nothing to receive
    *frames_received = 0;
    return NULL;
  } else {
    // ooh there's a buffer ready

    // if the previous one was processing, that means we've processed it.
    auto prev_process_index = (process_index == 0) ? (AUDIO_NB_BUFFERS - 1) : (process_index - 1);
    if (buffer_state[prev_process_index] == AUDIO_BUFFER_PROCESSING) {
      buffer_state[prev_process_index] = AUDIO_BUFFER_PROCESSED;
    }

    uint8_t *source = buffers + (process_index * buffer_size);
    *frames_received = AUDIO_NB_SAMPLES;
    buffer_state[process_index] = AUDIO_BUFFER_PROCESSING;
    process_index = (process_index + 1) % AUDIO_NB_BUFFERS;
    return source;
  }
}

void PulseReceiver::stop() {
  {
    lock_guard<mutex> lock(pa_mutex);
    if (ctx) {
      _pa_simple_free(ctx);
    }
    ctx = nullptr;
  }

  pa_thread->join();
}

PulseReceiver::~PulseReceiver() {
  if (in_buffer) {
    free(in_buffer);
  }
  if (buffers) {
    free(buffers);
  }
}
