
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

#include "./fps_counter.h"

#include <lab/logging.h>

#include <chrono>

#include <microprofile.h>

MICROPROFILE_DEFINE(EncoderMain, "Encoder", "Main", MP_WHITE);
MICROPROFILE_DEFINE(EncoderCycle, "Encoder", "Cycle", MP_WHITE);

MICROPROFILE_DEFINE(EncoderReceiveVideoFrame, "Encoder", "VRecv", MP_AQUAMARINE3);
MICROPROFILE_DEFINE(EncoderScale, "Encoder", "VScale", MP_THISTLE3);
MICROPROFILE_DEFINE(EncoderSendVideoFrame, "Encoder", "VEncode", MP_AZURE3);
MICROPROFILE_DEFINE(EncoderRecvVideoPkt, "Encoder", "VMux1", MP_BURLYWOOD3);
MICROPROFILE_DEFINE(EncoderWriteVideoPkt, "Encoder", "VMux2", MP_BROWN3);

MICROPROFILE_DEFINE(EncoderReceiveAudioFrames, "Encoder", "ARecv", MP_AQUAMARINE4);
MICROPROFILE_DEFINE(EncoderResample, "Encoder", "AResample", MP_THISTLE4);
MICROPROFILE_DEFINE(EncoderSendAudioFrames, "Encoder", "AEncode", MP_AZURE4);
MICROPROFILE_DEFINE(EncoderRecvAudioPkt, "Encoder", "AMux1", MP_BURLYWOOD4);
MICROPROFILE_DEFINE(EncoderWriteAudioPkt, "Encoder", "AMux2", MP_BROWN4);

namespace capsule {
namespace encoder {

void Run(capsule_args_t *args, encoder_params_t *params) {
  MicroProfileOnThreadCreate("encoder");
  MICROPROFILE_SCOPE(EncoderMain);

  int ret;

  av_register_all();

  if (args->debug_av) {
    av_log_set_level(AV_LOG_DEBUG);
  }

  // receive video format info
  video_format_t vfmt_in;
  ret = params->receive_video_format(params->private_data, &vfmt_in);
  if (ret != 0) {
    printf("could not receive video format");
    exit(1);
  }
  int width = (int) vfmt_in.width;
  int height = (int) vfmt_in.height;
  int components = 4;
  int linesize = vfmt_in.pitch;

  CapsuleLog("video resolution: %dx%d, format %d, vflip %d, pitch %d (%d computed)",
    width, height, (int) vfmt_in.format, (int) vfmt_in.vflip,
    (int) linesize, (int) (width * components));

  const int buffer_size = height * linesize;
  uint8_t *buffer = (uint8_t*) malloc(buffer_size);
  if (!buffer) {
    CapsuleLog("could not allocate buffer");
    exit(1);
  }

  // receive audio format info
  audio_format_t afmt_in;
  if (params->has_audio) {
    ret = params->receive_audio_format(params->private_data, &afmt_in);
    if (ret != 0) {
      CapsuleLog("could not receive audio format");
      exit(1);
    }

    CapsuleLog("audio format: %d channels, %d samplerate, %d samplewidth",
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

  const char *output_path = "capsule.mp4";

  fmt = av_guess_format("mp4", NULL, NULL);

  // allocate output media context
  avformat_alloc_output_context2(&oc, fmt, NULL, NULL);
  if (!oc) {
      CapsuleLog("could not allocate output context");
      exit(1);
  }
  oc->oformat = fmt;

  /* open the output file, if needed */
  ret = avio_open(&oc->pb, output_path, AVIO_FLAG_WRITE);
  if (ret < 0) {
      CapsuleLog("Could not open '%s'", output_path);
      exit(1);
  }

  // video stream
  video_st = avformat_new_stream(oc, NULL);
  if (!video_st) {
      CapsuleLog("could not allocate video stream");
      exit(1);
  }
  video_st->id = oc->nb_streams - 1;

  // audio stream
  if (params->has_audio) {
    audio_st = avformat_new_stream(oc, NULL);
    if (!audio_st) {
        CapsuleLog("could not allocate audio stream");
        exit(1);
    }
    audio_st->id = oc->nb_streams - 1;
  }

  // video codec
  vcodec = avcodec_find_encoder(vcodec_id);
  if (!vcodec) {
    CapsuleLog("could not find video codec");
    exit(1);
  }

  CapsuleLog("found video codec");

  vc = avcodec_alloc_context3(vcodec);
  if (!vc) {
      CapsuleLog("could not allocate video codec context");
      exit(1);
  }

  vc->codec_id = vcodec_id;
  vc->codec_type = AVMEDIA_TYPE_VIDEO;
  vc->pix_fmt = AV_PIX_FMT_YUV420P;

  if (args->pix_fmt) {
    if (0 == strcmp(args->pix_fmt, "yuv420p")) {
      vc->pix_fmt = AV_PIX_FMT_YUV420P;
    } else if (0 == strcmp(args->pix_fmt, "yuv444p")) {
      vc->pix_fmt = AV_PIX_FMT_YUV444P;
    } else {
      CapsuleLog("Unknown pix_fmt specified: %s - using default", args->pix_fmt);
    }
  }

  bool do_swscale = true;
  if (vfmt_in.format == capsule::kPixFmtYuv444P) {
    CapsuleLog("GPU color conversion enabled, ignoring user output settings and picking yuv444p");
    vc->pix_fmt = AV_PIX_FMT_YUV444P;
    do_swscale = false;
  }

  // temporarily disabled codepath as we're going to try gpu scalign or nothing
  int divider = 1;
  // if (args->divider != 0) {
  //   if (args->divider == 2 || args->divider == 4) {
  //     divider = args->divider;
  //   } else {
  //     CapsuleLog("Invalid size divider %d: must be 2 or 4. Ignoring...", args->divider);
  //   }
  // }

  int out_width = width / divider;
  int out_height = height / divider;

  // resolution must be a multiple of two
  if (out_width % 2 != 0) {
    out_width++;
  }
  if (out_height % 2 != 0) {
    out_height++;
  }

  CapsuleLog("output resolution: %dx%d", out_width, out_height);

  vc->width = out_width;
  vc->height = out_height;
  // frames per second - pts is in microseconds
  video_st->time_base = AVRational{1,1000000};
  vc->time_base = video_st->time_base;

  vc->gop_size = 120;
  if (args->gop_size) {
    vc->gop_size = args->gop_size;
  }

  vc->max_b_frames = 16;
  if (args->max_b_frames) {
    vc->max_b_frames = args->max_b_frames;
  }

  vc->rc_buffer_size = 0;

  // H264
  int crf = 20;
  if (args->crf != -1) {
    if (args->crf >= 0 && args->crf <= 51) {
      if (args->crf < 18 || args->crf > 28) {
        CapsuleLog("Warning: sane crf values lie within 18-28, using crf %d at your own risks", args->crf)
      }
      crf = args->crf;
    } else {
      CapsuleLog("Invalid crf value %d (must be in the 0-51 range), ignoring", args->crf)
    }
  }

  vc->qmin = crf;
  vc->qmax = crf;

  // multithreading
  vc->thread_count = 1;
  if (args->threads) {
    if (args->threads > 0 && args->threads <= 32) {
      vc->thread_count = args->threads;
    } else {
      CapsuleLog("Invalid threads parameter %d, ignoring", args->threads)
    }
  }

  if (vc->thread_count > 1) {
    CapsuleLog("Activating frame-level threading with %d threads", vc->thread_count);
    vc->thread_type = FF_THREAD_FRAME;
  }

  vc->flags |= CODEC_FLAG_GLOBAL_HEADER;

  if (vc->pix_fmt == AV_PIX_FMT_YUV444P) {
    CapsuleLog("Warning: can't use baseline because yuv444p colorspace selected. Encoding will take more CPU.")
  } else {
    vc->profile = FF_PROFILE_H264_BASELINE;
  }

  const char *preset = "ultrafast";
  if (args->x264_preset) {
    preset = args->x264_preset;
  }
  av_opt_set(vc->priv_data, "preset", preset, AV_OPT_SEARCH_CHILDREN);

  ret = avcodec_open2(vc, vcodec, NULL);
  if (ret < 0) {
    CapsuleLog("could not open video codec");
    exit(1);
  }

  ret = avcodec_parameters_from_context(video_st->codecpar, vc);
  if (ret < 0) {
    CapsuleLog("could not copy video codec parameters");
    exit(1);
  }

  // audio codec 
  if (params->has_audio) {
    acodec = avcodec_find_encoder(acodec_id);
    if (!acodec) {
      CapsuleLog("could not find audio codec");
      exit(1);
    }

    CapsuleLog("found audio codec");

    ac = avcodec_alloc_context3(acodec);
    if (!ac) {
        CapsuleLog("could not allocate audio codec context");
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
      CapsuleLog("could not open audio codec");
      exit(1);
    }

    ret = avcodec_parameters_from_context(audio_st->codecpar, ac);
    if (ret < 0) {
      CapsuleLog("could not copy audio codec parameters");
      exit(1);
    }
  }

  // video frame
  vframe = av_frame_alloc();
  if (!vframe) {
    CapsuleLog("could not allocate video frame");
    exit(1);
  }
  vframe->format = vc->pix_fmt;
  vframe->width = vc->width;
  vframe->height = vc->height;

  AVPixelFormat vpix_fmt;
  switch (vfmt_in.format) {
    case capsule::kPixFmtRgba:
      vpix_fmt = AV_PIX_FMT_RGBA;
      break;
    case capsule::kPixFmtBgra:
      vpix_fmt = AV_PIX_FMT_BGRA;
      break;
    case capsule::kPixFmtRgb10A2:
      vpix_fmt = AV_PIX_FMT_RGBA;
      break;
    case capsule::kPixFmtYuv444P:
      // no conversion actually required
      vpix_fmt = AV_PIX_FMT_YUV444P;
      break;
    default:
      printf("Unknown video format %d, bailing out\n", vfmt_in.format);
  }

  // audio frame
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

  // TODO: just use vfmt specs instead of handling vflip here
  int sws_linesize[1];
  uint8_t *sws_in[1];

  if (do_swscale) {
    // initialize swscale context
    sws = sws_getContext(
      // input
      width, height, vpix_fmt,
      // output
      vframe->width, vframe->height, vc->pix_fmt,
      // ???
      0, 0, 0, 0
    );

    int vflip = vfmt_in.vflip;

    if (vflip) {
      sws_in[0] = buffer + linesize*(height-1);
      sws_linesize[0] = -linesize;
    } else {
      // specify negative stride to flip
      sws_in[0] = buffer;
      sws_linesize[0] = linesize;
    }

    // FIXME: just messing around
    bool misalign_planes = !!getenv("MISALIGN_PLANES");
    if (misalign_planes) {
      CapsuleLog("Purposefully misaligning planes to confirm suspicions about x264 performance");
      size_t frame_buffer_size = vc->width * 4 * vc->height;
      uint8_t *frame_buffer = (uint8_t *) malloc(frame_buffer_size);
      vframe->data[0] = frame_buffer;
      vframe->data[1] = frame_buffer + vc->width;
      vframe->data[2] = frame_buffer + vc->width * 2;
      vframe->linesize[0] = vc->width * 4;
      vframe->linesize[1] = vc->width * 4;
      vframe->linesize[2] = vc->width * 4;
    } else {
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
    }
  } else {
    // FIXME: use vfmt offsets & linesizes instead of computing them here
    // this assumes a horizontal format, see https://twitter.com/fasterthanlime/status/839086194919161857
    vframe->data[0] = buffer;
    vframe->data[1] = buffer + width;
    vframe->data[2] = buffer + width * 2;
    vframe->linesize[0] = width * 4;
    vframe->linesize[1] = width * 4;
    vframe->linesize[2] = width * 4;
  }

  CapsuleLog("initial offsets: %p %p (%" PRIdS " from first) %p (%" PRIdS " from first), linesize: %d %d %d",
    vframe->data[0],
    vframe->data[1], vframe->data[1] - vframe->data[0],
    vframe->data[2], vframe->data[2] - vframe->data[0],
    vframe->linesize[0],
    vframe->linesize[1],
    vframe->linesize[2]
  );

  // initialize swrescale context
  // FIXME: we're actually never using that for now
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

  size_t last_print_read = 0;

  vframe->pts = 0;
  int vnext_pts = 0;
  int anext_pts = 0;

  int last_frame = 0;
  float factor = 1.0;
  int64_t timestamp = 0;
  int64_t first_timestamp = -1;
  int64_t last_timestamp = 0;
  FPSCounter fps_counter;

  int samples_received = 0;
  int samples_used = 0;
  int in_sample_size  = 0;
  float *in_samples;

  if (params->has_audio) {
    in_sample_size = afmt_in.channels * afmt_in.samplewidth / 8;
  }

  int samples_filled = 0;
  int frame_count = 0;

  while (true) {
    MICROPROFILE_SCOPE(EncoderCycle);

    size_t read;

    {
      MICROPROFILE_SCOPE(EncoderReceiveVideoFrame);
      read = params->receive_video_frame(params->private_data, buffer, buffer_size, &timestamp);
    }
    if (first_timestamp < 0) {
      first_timestamp = timestamp;
    }
    timestamp -= first_timestamp;

    auto delta = timestamp - last_timestamp;
    last_timestamp = timestamp;
    if (fps_counter.TickDelta(delta)) {
      fprintf(stderr, "FPS: %.2f\n", fps_counter.Fps());
      fflush(stderr);
    }

    if (read < buffer_size) {
      last_frame = true;
    } else {
      {
        MICROPROFILE_SCOPE(EncoderScale);
        if (do_swscale) {
          sws_scale(sws, sws_in, sws_linesize, 0, height, vframe->data, vframe->linesize);
        } else {
          // muffin
        }
      }

      vnext_pts = timestamp;
      vframe->pts = vnext_pts;

      if (frame_count < 3) {
        CapsuleLog("initial offsets: %p %p (%" PRIdS " from first) %p (%" PRIdS " from first), linesize: %d %d %d",
          vframe->data[0],
          vframe->data[1], vframe->data[1] - vframe->data[0],
          vframe->data[2], vframe->data[2] - vframe->data[0],
          vframe->linesize[0],
          vframe->linesize[1],
          vframe->linesize[2]
        );
      }

      // write video frame
      {
        MICROPROFILE_SCOPE(EncoderSendVideoFrame);
        ret = avcodec_send_frame(vc, vframe);
      }
      if (ret < 0) {
          fprintf(stderr, "Error encoding video frame\n");
          exit(1);
      }
    }

    while (ret >= 0) {
      AVPacket vpkt;
      av_init_packet(&vpkt);

      {
        MICROPROFILE_SCOPE(EncoderRecvVideoPkt);
        ret = avcodec_receive_packet(vc, &vpkt);
      }
      if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
          fprintf(stderr, "Error encoding a video frame\n");
          exit(1);
      } else if (ret >= 0) {
          // printf(">> vpkt timestamp before rescaling = %d, %.4f secs\n", (int) vpkt.pts, ((double) vpkt.pts) / 1000000.0);
          av_packet_rescale_ts(&vpkt, vc->time_base, video_st->time_base);
          // printf(">>                after  rescaling = %d, %.4f secs\n", (int) vpkt.pts, ((double) vpkt.pts) / 1000000.0);
          vpkt.stream_index = video_st->index;

          /* Write the compressed frame to the media file. */
          {
            MICROPROFILE_SCOPE(EncoderWriteVideoPkt);
            ret = av_interleaved_write_frame(oc, &vpkt);
          }
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

            {
              MICROPROFILE_SCOPE(EncoderReceiveAudioFrames);
              in_samples = (float *) params->receive_audio_frames(params->private_data, &samples_received);
            }
            if (samples_received == 0) {
              underrun = true;
              break;
            }
          }

          {
            MICROPROFILE_SCOPE(EncoderResample);
            while (samples_used < samples_received && samples_filled < samples_needed) {
              left_samples[samples_filled]  = in_samples[samples_used * 2];
              right_samples[samples_filled] = in_samples[samples_used * 2 + 1];
              samples_filled++;
              samples_used++;
            }
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

        {
          MICROPROFILE_SCOPE(EncoderSendAudioFrames);
          ret = avcodec_send_frame(ac, aframe);
        }
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
          {
            MICROPROFILE_SCOPE(EncoderRecvAudioPkt);
            ret = avcodec_receive_packet(ac, &apkt);
          }

          if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
              fprintf(stderr, "Error encoding an audio frame\n");
              exit(1);
          } else if (ret >= 0) {
              av_packet_rescale_ts(&apkt, ac->time_base, audio_st->time_base);
              apkt.stream_index = audio_st->index;
              /* Write the compressed audio frame to the media file. */
              {
                MICROPROFILE_SCOPE(EncoderWriteAudioPkt);
                ret = av_interleaved_write_frame(oc, &apkt);
              }
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
        {
          MICROPROFILE_SCOPE(EncoderRecvAudioPkt);
          ret = avcodec_receive_packet(ac, &apkt);
        }

        if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
            fprintf(stderr, "Error encoding an audio frame\n");
            exit(1);
        } else if (ret >= 0) {
            av_packet_rescale_ts(&apkt, ac->time_base, audio_st->time_base);
            apkt.stream_index = audio_st->index;
            /* Write the compressed audio frame to the media file. */
            {
              MICROPROFILE_SCOPE(EncoderWriteAudioPkt);
              ret = av_interleaved_write_frame(oc, &apkt);
            }
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

    frame_count++;
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
  if (do_swscale) {
    av_freep(&vframe->data[0]);
  } else {
    // don't free, we're just messing with avframe buffers
  }
  av_frame_free(&vframe);

  if (params->has_audio) {
    avcodec_close(ac);
    av_frame_free(&aframe);
  }

  avio_close(oc->pb);
  avformat_free_context(oc);

  // FIXME: seems to crash atm.
  // MicroProfileOnThreadExit();
}

} // namespace encoder
} // namespace capsule
