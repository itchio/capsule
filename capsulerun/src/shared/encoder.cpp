
#include "capsulerun.h"

extern "C" {
    #include <libavcodec/avcodec.h>

    #include <libavformat/avformat.h>

    #include <libavutil/mathematics.h>
    #include <libavutil/imgutils.h>
    #include <libavutil/opt.h>

    #include <libswscale/swscale.h>
    #include <libswresample/swresample.h>
}

#include <chrono>

using namespace std;

void encoder_run(encoder_params_t *params) {
  int ret;

  av_register_all();
#ifdef CAPSULERUN_PROFILE
  av_log_set_level(AV_LOG_DEBUG);
#endif

  // receive video format info
  video_format_t vfmt_in;
  ret = params->receive_video_format(params->private_data, &vfmt_in);
  if (ret != 0) {
    printf("could not receive video format");
    exit(1);
  }
  int64_t width = vfmt_in.width;
  int64_t height = vfmt_in.height;

  int components = 4;
  int linesize = vfmt_in.pitch;

  printf("video resolution: %dx%d, format %d, vflip %d, pitch %d (%d computed)\n",
    (int) width, (int) height, (int) vfmt_in.format, (int) vfmt_in.vflip,
    (int) linesize, (int) (width * components));

  const int buffer_size = height * linesize;
  uint8_t *buffer = (uint8_t*) malloc(buffer_size);
  if (!buffer) {
    printf("could not allocate buffer\n");
    exit(1);
  }

  // receive audio format info
  audio_format_t afmt_in;
  if (params->has_audio) {
    ret = params->receive_audio_format(params->private_data, &afmt_in);
    if (ret != 0) {
      printf("could not receive audio format");
      exit(1);
    }

    printf("audio format: %d channels, %d samplerate, %d samplewidth\n",
      afmt_in.channels, afmt_in.samplerate, afmt_in.samplewidth);
  }

  AVFormatContext *oc;
  AVOutputFormat *fmt;

  AVStream *video_st, *audio_st;

  AVCodecID vcodec_id = AV_CODEC_ID_H264;
  AVCodecID acodec_id = AV_CODEC_ID_AAC;
  AVCodec *vcodec, *acodec;
  AVCodecContext *vc, *ac;

  AVFrame *vframe, *aframe;

  struct SwsContext *sws;
  struct SwrContext *swr;

  double video_time;

  const char *output_path = "capsule.mp4";

  fmt = av_guess_format("mp4", NULL, NULL);

  // allocate output media context
  avformat_alloc_output_context2(&oc, fmt, NULL, NULL);
  if (!oc) {
      printf("could not allocate output context\n");
      exit(1);
  }
  oc->oformat = fmt;

  /* open the output file, if needed */
  ret = avio_open(&oc->pb, output_path, AVIO_FLAG_WRITE);
  if (ret < 0) {
      fprintf(stderr, "Could not open '%s'\n", output_path);
      exit(1);
  }

  // video stream
  video_st = avformat_new_stream(oc, NULL);
  if (!video_st) {
      printf("could not allocate video stream\n");
      exit(1);
  }
  video_st->id = oc->nb_streams - 1;

  // audio stream
  if (params->has_audio) {
    audio_st = avformat_new_stream(oc, NULL);
    if (!audio_st) {
        printf("could not allocate audio stream\n");
        exit(1);
    }
    audio_st->id = oc->nb_streams - 1;
  }

  // video codec
  vcodec = avcodec_find_encoder(vcodec_id);
  if (!vcodec) {
    printf("could not find video codec\n");
    exit(1);
  }

  printf("found video codec\n");

  vc = avcodec_alloc_context3(vcodec);
  if (!vc) {
      printf("could not allocate video codec context\n");
      exit(1);
  }

  vc->codec_id = vcodec_id;
  vc->codec_type = AVMEDIA_TYPE_VIDEO;
  vc->pix_fmt = AV_PIX_FMT_YUV420P; // damn you twitter
  // vc->pix_fmt = AV_PIX_FMT_YUV444P;

  // resolution must be a multiple of two
  vc->width = width;
  vc->height = height;
  // frames per second - pts is in microseconds
  video_st->time_base = AVRational{1,1000000};
  vc->time_base = video_st->time_base;

  vc->gop_size = 120;
  vc->max_b_frames = 16;
  vc->rc_buffer_size = 0;

  // H264
  vc->qmin = 20;
  vc->qmax = 20;

  vc->flags |= CODEC_FLAG_GLOBAL_HEADER;

  // see also "placebo" and "ultrafast" presets
  // av_opt_set(vc->priv_data, "preset", "veryfast", AV_OPT_SEARCH_CHILDREN);
  av_opt_set(vc->priv_data, "preset", "ultrafast", AV_OPT_SEARCH_CHILDREN);

  ret = avcodec_open2(vc, vcodec, NULL);
  if (ret < 0) {
    printf("could not open video codec\n");
    exit(1);
  }

  ret = avcodec_parameters_from_context(video_st->codecpar, vc);
  if (ret < 0) {
    printf("could not copy video codec parameters\n");
    exit(1);
  }

  // audio codec 
  if (params->has_audio) {
    AVCodec *acodec = avcodec_find_encoder(acodec_id);
    if (!acodec) {
      printf("could not find audio codec\n");
      exit(1);
    }

    printf("found audio codec\n");

    ac = avcodec_alloc_context3(acodec);
    if (!ac) {
        printf("could not allocate audio codec context\n");
        exit(1);
    }

    ac->bit_rate = 128000;
    ac->sample_fmt = AV_SAMPLE_FMT_FLTP;
    ac->sample_rate = afmt_in.samplerate;
    ac->channels = afmt_in.channels;
    ac->channel_layout = AV_CH_LAYOUT_STEREO;

    audio_st->time_base = AVRational{1,ac->sample_rate};
    ac->time_base = audio_st->time_base;

    ret = avcodec_open2(ac, acodec, NULL);
    if (ret < 0) {
      printf("could not open audio codec\n");
      exit(1);
    }

    ret = avcodec_parameters_from_context(audio_st->codecpar, ac);
    if (ret < 0) {
      printf("could not copy audio codec parameters\n");
      exit(1);
    }
  }

  // video frame
  vframe = av_frame_alloc();
  if (!vframe) {
    printf("could not allocate video frame\n");
    exit(1);
  }
  vframe->format = vc->pix_fmt;
  vframe->width = vc->width;
  vframe->height = vc->height;

  AVPixelFormat vpix_fmt;
  switch (vfmt_in.format) {
    case CAPSULE_PIX_FMT_RGBA:
      vpix_fmt = AV_PIX_FMT_RGBA;
      break;
    case CAPSULE_PIX_FMT_BGRA:
      vpix_fmt = AV_PIX_FMT_BGRA;
      break;
    default:
      printf("Unknown video format %d, bailing out\n", vfmt_in.format);
  }

  // audio frame
  uint8_t *audio_buffer;
  int audio_buffer_size;

  if (params->has_audio) {
    aframe = av_frame_alloc();
    if (!aframe) {
      printf("could not allocate audio frame\n");
      exit(1);
    }

    aframe->format = ac->sample_fmt;
    aframe->channel_layout = ac->channel_layout;
    aframe->sample_rate = ac->sample_rate;
    aframe->nb_samples = ac->frame_size;

    ret = av_frame_get_buffer(aframe, 0);
    if (ret < 0) {
      fprintf(stderr, "could not allocate audio frame buffer\n");
      exit(1);
    }
  }

  // initialize swscale context
  sws = sws_getContext(
    // input
    vframe->width, vframe->height, vpix_fmt,
    // output
    vframe->width, vframe->height, vc->pix_fmt,
    // ???
    0, 0, 0, 0
  );

  int sws_linesize[1];
  uint8_t *sws_in[1];

  int vflip = vfmt_in.vflip;

  if (vflip) {
    sws_in[0] = buffer + linesize*(height-1);
    sws_linesize[0] = -linesize;
  } else {
    // specify negative stride to flip
    sws_in[0] = buffer;
    sws_linesize[0] = linesize;
  }

  /* the image can be allocated by any means and av_image_alloc() is
   * just the most convenient way if av_malloc() is to be used */
  ret = av_image_alloc(
      vframe->data,
      vframe->linesize,
      vc->width,
      vc->height,
      vc->pix_fmt,
      32 /* alignment */
  );
  if (ret < 0) {
    fprintf(stderr, "Could not allocate raw picture buffer\n");
    exit(1);
  }

  // initialize swrescale context
  if (params->has_audio) {
    swr = swr_alloc();
    if (!swr) {
      fprintf(stderr, "could not allocate resampling context\n");
      exit(1);
    }

    av_opt_set_int(swr, "in_channel_layout",    AV_CH_LAYOUT_STEREO, 0);
    av_opt_set_int(swr, "in_sample_rate",       ac->sample_rate, 0);
    av_opt_set_sample_fmt(swr, "in_sample_fmt", AV_SAMPLE_FMT_S32, 0);
    av_opt_set_int(swr, "out_channel_layout",    AV_CH_LAYOUT_STEREO, 0);
    av_opt_set_int(swr, "out_sample_rate",       ac->sample_rate, 0);
    av_opt_set_sample_fmt(swr, "out_sample_fmt", ac->sample_fmt, 0);

    ret = swr_init(swr);
    if (ret < 0) {
      fprintf(stderr, "could not initialize resampling context\n");
      exit(1);
    }
  }


  av_dump_format(oc, 0, output_path, 1);

  // write stream header, if any
  ret = avformat_write_header(oc, NULL);
  if (ret < 0) {
    printf("Error occured when opening output file\n");
    exit(1);
  }

  size_t total_read = 0;
  size_t last_print_read = 0;

  vframe->pts = 0;
  int vnext_pts = 0;
  int anext_pts = 0;

  int last_frame = 0;
  float factor = 1.0;
  int64_t timestamp = 0;
  int64_t last_timestamp = 0;
  const int FPS_SAMPLE_SIZE = 30;
  float fps_samples[FPS_SAMPLE_SIZE];
  for (int i = 0; i < FPS_SAMPLE_SIZE; i++) {
    fps_samples[i] = 0.0f;
  }
  int current_fps_sample = 0;

  int samples_received = 0;
  int samples_used = 0;
  int in_sample_size = afmt_in.channels * afmt_in.samplewidth / 8;
  float *in_samples;

  int samples_filled = 0;

  while (true) {
    auto tt1 = chrono::steady_clock::now();
    size_t read = params->receive_video_frame(params->private_data, buffer, buffer_size, &timestamp);
    auto tt2 = chrono::steady_clock::now();
    eprintf("receive_frame took %.3f ms\n", (double) (chrono::duration_cast<chrono::microseconds>(tt2 - tt1).count()) / 1000.0);
    dprintf(">> video timestamp                 = %d, approx %.4f seconds\n", (int) timestamp, ((double) timestamp) / 1000000.0);
    total_read += read;

    int delta = (timestamp - last_timestamp);
    last_timestamp = timestamp;
    if (delta == 0) {
      delta = 16666;
    }
    float fps = 1000000.0f / (float) delta;
    fps_samples[current_fps_sample] = fps;
    current_fps_sample = (current_fps_sample + 1) % FPS_SAMPLE_SIZE;
    float mean_fps = 0.0f;
    for (int i = 0; i < FPS_SAMPLE_SIZE; i++) {
      mean_fps += fps_samples[i];
    }
    mean_fps /= (float) FPS_SAMPLE_SIZE;

    fprintf(stderr, "FPS: %.2f\n", mean_fps);
    fflush(stderr);

    if (read < buffer_size) {
      last_frame = true;
    } else {
      auto ht1 = chrono::steady_clock::now();
      sws_scale(sws, sws_in, sws_linesize, 0, vc->height, vframe->data, vframe->linesize);
      auto ht2 = chrono::steady_clock::now();
      eprintf("scale took %.3f ms\n", (double) (chrono::duration_cast<chrono::microseconds>(ht2 - ht1).count()) / 1000.0);

      vnext_pts = timestamp;
      vframe->pts = vnext_pts;

      // write video frame
      {
        auto t1 = chrono::steady_clock::now();
        ret = avcodec_send_frame(vc, vframe);
        auto t2 = chrono::steady_clock::now();
        eprintf("send_frame took %.3f ms\n", (double) (chrono::duration_cast<chrono::microseconds>(t2 - t1).count()) / 1000.0);
      }
      if (ret < 0) {
          fprintf(stderr, "Error encoding video frame\n");
          exit(1);
      }
    }

    while (ret >= 0) {
      AVPacket vpkt;
      av_init_packet(&vpkt);

      ret = avcodec_receive_packet(vc, &vpkt);
      if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
          fprintf(stderr, "Error encoding a video frame\n");
          exit(1);
      } else if (ret >= 0) {
          // printf(">> vpkt timestamp before rescaling = %d, %.4f secs\n", (int) vpkt.pts, ((double) vpkt.pts) / 1000000.0);
          av_packet_rescale_ts(&vpkt, vc->time_base, video_st->time_base);
          // printf(">>                after  rescaling = %d, %.4f secs\n", (int) vpkt.pts, ((double) vpkt.pts) / 1000000.0);
          vpkt.stream_index = video_st->index;

          /* Write the compressed frame to the media file. */
          ret = av_interleaved_write_frame(oc, &vpkt);
          if (ret < 0) {
              fprintf(stderr, "Error while writing video frame\n");
              exit(1);
          }
      }
    }

    if (params->has_audio) {
      int audio_frames_copied = 0;

      // while video frame is ahead of audio
      while (av_compare_ts(vframe->pts, video_st->time_base, aframe->pts, audio_st->time_base) >= 0) {
        int samples_needed = aframe->nb_samples;
        int underrun = 0;

        ret = av_frame_make_writable(aframe);
        if (ret < 0) {
          fprintf(stderr, "Could not make audio frame writable\n");
          exit(1);
        }

        float *left_samples = (float *) aframe->data[0];
        float *right_samples = (float *) aframe->data[1];

        while (samples_filled < samples_needed) {
          if (samples_used >= samples_received) {
            samples_used = 0;

            in_samples = (float *) params->receive_audio_frames(params->private_data, &samples_received);
            if (samples_received == 0) {
              underrun = true;
              break;
            }
          }

          while (samples_used < samples_received && samples_filled < samples_needed) {
            left_samples[samples_filled]  = in_samples[samples_used * 2];
            right_samples[samples_filled] = in_samples[samples_used * 2 + 1];
            samples_filled++;
            samples_used++;
          }
        }

        if (underrun) {
          // we'll get more frames next time, no biggie
          break;
        }

        audio_frames_copied++;

        aframe->pts = anext_pts;
        anext_pts += aframe->nb_samples;

        samples_filled = 0;

        ret = avcodec_send_frame(ac, aframe);
        if (ret < 0) {
          const int err_string_size = 16 * 1024;
          char err_string[err_string_size];
          err_string[0] = '\0';
          av_strerror(ret, err_string, err_string_size);
          printf("Error encoding audio frame: error %d (%x) - %s\n", ret, ret, err_string);
          exit(1);
        }

        while (ret >= 0) {
          AVPacket apkt;
          av_init_packet(&apkt);
          ret = avcodec_receive_packet(ac, &apkt);

          if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
              fprintf(stderr, "Error encoding an audio frame\n");
              exit(1);
          } else if (ret >= 0) {
              av_packet_rescale_ts(&apkt, ac->time_base, audio_st->time_base);
              apkt.stream_index = audio_st->index;
              /* Write the compressed audio frame to the media file. */
              ret = av_interleaved_write_frame(oc, &apkt);
              if (ret < 0) {
                  fprintf(stderr, "Error while writing audio frame\n");
                  exit(1);
              }
          }
        }
      }

      // printf("Copied %d audio frames\n", audio_frames_copied);

      while (ret >= 0) {
        AVPacket apkt;
        av_init_packet(&apkt);
        ret = avcodec_receive_packet(ac, &apkt);

        if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
            fprintf(stderr, "Error encoding an audio frame\n");
            exit(1);
        } else if (ret >= 0) {
            av_packet_rescale_ts(&apkt, ac->time_base, audio_st->time_base);
            apkt.stream_index = audio_st->index;
            /* Write the compressed audio frame to the media file. */
            ret = av_interleaved_write_frame(oc, &apkt);
            if (ret < 0) {
                fprintf(stderr, "Error while writing audio frame\n");
                exit(1);
            }
        }
      }
    }

    if (last_frame) {
      break;
    }
  }

  // delayed video frames
  ret = avcodec_send_frame(vc, NULL);
  if (ret < 0) {
    fprintf(stderr, "couldn't flush video codec\n");
    exit(1);
  }

  while (ret >= 0) {
    AVPacket vpkt;
    av_init_packet(&vpkt);
    ret = avcodec_receive_packet(vc, &vpkt);

    if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
        fprintf(stderr, "Error encoding a video frame\n");
        exit(1);
    } else if (ret >= 0) {
        // printf(">> vpkt timestamp before rescaling = %d, %.4f secs\n", (int) vpkt.pts, ((double) vpkt.pts) / 1000000.0);
        av_packet_rescale_ts(&vpkt, vc->time_base, video_st->time_base);
        // printf(">>                after  rescaling = %d, %.4f secs\n", (int) vpkt.pts, ((double) vpkt.pts) / 1000000.0);
        vpkt.stream_index = video_st->index;
        /* Write the compressed frame to the media file. */
        ret = av_interleaved_write_frame(oc, &vpkt);
        if (ret < 0) {
            fprintf(stderr, "Error while writing video frame\n");
            exit(1);
        }
    }
  }

  // delayed audio frames
  if (params->has_audio) {
    ret = avcodec_send_frame(ac, NULL);
    if (ret < 0) {
      fprintf(stderr, "couldn't flush audio codec\n");
      exit(1);
    }

    while (ret >= 0) {
      AVPacket apkt;
      av_init_packet(&apkt);
      ret = avcodec_receive_packet(ac, &apkt);

      if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
          fprintf(stderr, "Error encoding a audio frame\n");
          exit(1);
      } else if (ret >= 0) {
          av_packet_rescale_ts(&apkt, ac->time_base, audio_st->time_base);
          apkt.stream_index = audio_st->index;
          /* Write the compressed frame to the media file. */
          ret = av_interleaved_write_frame(oc, &apkt);
          if (ret < 0) {
              fprintf(stderr, "Error while writing audio frame\n");
              exit(1);
          }
      }
    }
  }

  // Write format trailer if any
  ret = av_write_trailer(oc);
  if (ret < 0) {
    printf("failed to write trailer\n");
    exit(1);
  }

  avcodec_close(vc);
  av_freep(&vframe->data[0]);
  av_frame_free(&vframe);

  if (params->has_audio) {
    avcodec_close(ac);
    av_frame_free(&aframe);
  }

  avio_close(oc->pb);
  avformat_free_context(oc);
}

