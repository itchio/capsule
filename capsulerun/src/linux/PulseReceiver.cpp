#include "PulseReceiver.h"

#include <stdexcept>

#include <string.h> // memset, memcpy

// #define AUDIO_NB_SAMPLES 1024
#define AUDIO_NB_SAMPLES 128

using namespace std;

PulseReceiver::PulseReceiver() {
  memset(&afmt, 0, sizeof(audio_format_t));

  /* The sample type to use */
  static const pa_sample_spec ss = {
      .format = PA_SAMPLE_FLOAT32LE, .rate = 44100, .channels = 2};

  // TODO: list devices instead
  const char *dev = "alsa_output.pci-0000_00_1b.0.analog-stereo.monitor";

  int pa_err = 0;
  ctx = pa_simple_new(NULL,             // server
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
    throw runtime_error("could not create pulseaudio context");
  }

  afmt.channels = ss.channels;
  afmt.samplerate = ss.rate;
  afmt.samplewidth = 32;
  audio_buffer_size =
      AUDIO_NB_SAMPLES * afmt.channels * (afmt.samplewidth / 8);
  audio_buffer = (uint8_t *) malloc(audio_buffer_size);
  memset(audio_buffer, 0, audio_buffer_size);
}

int PulseReceiver::receiveFormat(audio_format_t *afmt_out) {
  memcpy(afmt_out, &afmt, sizeof(audio_format_t));
  return 0;
}

void *PulseReceiver::receiveFrames(int *frames_received) {
  lock_guard<mutex> lock(pa_mutex);

  if (!ctx) {
    // we're closed!'
    *frames_received = 0;
    return NULL;
  }

  int pa_err;
  int ret = pa_simple_read(ctx, audio_buffer, audio_buffer_size, &pa_err);
  if (ret < 0) {
      fprintf(stderr, "Could not read from pulseaudio, error %d (%x)\n", pa_err, pa_err);
      *frames_received = 0;
      return NULL;
  }

  *frames_received = AUDIO_NB_SAMPLES;
  return audio_buffer;
}

void PulseReceiver::stop() {
  lock_guard<mutex> lock(pa_mutex);

  // TODO: what about concurrency?
  pa_simple_free(ctx);
  ctx = nullptr;
}

PulseReceiver::~PulseReceiver() {
  free(audio_buffer);
}
