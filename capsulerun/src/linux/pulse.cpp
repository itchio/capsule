
#include "capsulerun.h"

#define AUDIO_NB_SAMPLES 1024

int receive_audio_format (encoder_private_t *p, audio_format_t *fmt) {
    /* The sample type to use */
    static const pa_sample_spec ss = {
        .format = PA_SAMPLE_FLOAT32LE,
        .rate = 44100,
        .channels = 2
    };

    // TODO: list devices instead
    const char *dev = "alsa_output.pci-0000_00_1b.0.analog-stereo.monitor";

    int pa_err = 0;
    p->pactx = pa_simple_new(
        NULL, // server
        "capsule", // name
        PA_STREAM_RECORD, // direction
        dev, // device
        "record", // stream name
        &ss, // sample spec
        NULL, // channel map
        NULL, // buffer attributes
        &pa_err // error
    );

    if (!p->pactx) {
        fprintf(stderr, "Could not create pulseaudio context, error %d (%x)\n", pa_err, pa_err);
        return 1;
    }

    fmt->channels = ss.channels;
    fmt->samplerate = ss.rate;
    fmt->samplewidth = 32;
    p->audio_buffer_size = AUDIO_NB_SAMPLES * fmt->channels * (fmt->samplewidth / 8);
    p->audio_buffer = (uint8_t *) malloc(p->audio_buffer_size);
    memset(p->audio_buffer, 0, p->audio_buffer_size);
    return 0;
}

void *receive_audio_frames (encoder_private_t *p, int *frames_received) {
  int pa_err;
  int ret = pa_simple_read(p->pactx, p->audio_buffer, p->audio_buffer_size, &pa_err);
  if (ret < 0) {
      fprintf(stderr, "Could not read from pulseaudio, error %d (%x)\n", pa_err, pa_err);
      return NULL;
  }

  *frames_received = AUDIO_NB_SAMPLES;
  return p->audio_buffer;
}
