
#include "capsulerun.h"

extern "C" {
    #include <libavcodec/avcodec.h>

    #include <libavformat/avformat.h>

    #include <libavutil/mathematics.h>
    #include <libavutil/imgutils.h>
    #include <libavutil/opt.h>

    #include <libswscale/swscale.h>
}

void encoder_run(encoder_params_t *params) {
  int ret;

  av_register_all();
  av_log_set_level(AV_LOG_DEBUG);

  int64_t width, height;
  ret = params->receive_resolution(params->private_data, &width, &height);
  if (ret != 0) {
    printf("could not receive resolution");
    exit(1);
  }

  printf("resolution: %dx%d\n", (int) width, (int) height);

  // assuming BGRA
  int components = 4;
  const int buffer_size = width * height * components;
  uint8_t *buffer = (uint8_t*) malloc(buffer_size);
  if (!buffer) {
    printf("could not allocate buffer\n");
    exit(1);
  }

  int linesize = width * components;

  AVFormatContext *oc;
  AVOutputFormat *fmt;
  AVStream *video_st;
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

  video_st = avformat_new_stream(oc, 0);
  if (!video_st) {
      printf("could not allocate stream\n");
      exit(1);
  }
  video_st->id = oc->nb_streams - 1;

  AVCodecID vcodec_id = AV_CODEC_ID_H264;

  AVCodec *vcodec = avcodec_find_encoder(vcodec_id);
  if (!vcodec) {
    printf("could not find video codec\n");
    exit(1);
  }

  printf("found video codec\n");

  AVCodecContext *vc = avcodec_alloc_context3(vcodec);
  if (!vc) {
      printf("could not allocate codec context\n");
      exit(1);
  }

  vc->codec_id = vcodec_id;
  vc->codec_type = AVMEDIA_TYPE_VIDEO;
  vc->pix_fmt = AV_PIX_FMT_YUV420P;

  // sample parameters
  vc->bit_rate = 5000000;
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
  vc->qmin = 10;
  vc->qmax = 51;

  vc->flags |= CODEC_FLAG_GLOBAL_HEADER;

  // see also "placebo" and "ultrafast" presets
  av_opt_set(vc->priv_data, "preset", "veryfast", AV_OPT_SEARCH_CHILDREN);

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

  AVFrame *vframe = av_frame_alloc();
  if (!vframe) {
    printf("could not allocate video frame\n");
  }
  vframe->format = vc->pix_fmt;
  vframe->width = vc->width;
  vframe->height = vc->height;

  // initialize swscale context
  struct SwsContext *sws = sws_getContext(
    // input
    vframe->width, vframe->height, AV_PIX_FMT_BGRA,
    // output
    vframe->width, vframe->height, vc->pix_fmt,
    // ???
    0, 0, 0, 0
  );

  int sws_linesize[1];
  uint8_t *sws_in[1];

  int vflip = 1;

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

  av_dump_format(oc, 0, output_path, 1);

  // write stream header, if any
  ret = avformat_write_header(oc, NULL);
  if (ret < 0) {
    printf("Error occured when opening output file\n");
    exit(1);
  }

  size_t total_read = 0;
  size_t last_print_read = 0;

  int x, y;
  int got_output;

  vframe->pts = 0;
  int vnext_pts = 0;

  while (true) {
    int64_t delta;
    size_t read = params->receive_frame(params->private_data, buffer, buffer_size, &delta);
    total_read += read;

    if (read < buffer_size) {
      printf("received last frame\n");
      break;
    }

    sws_scale(sws, sws_in, sws_linesize, 0, vc->height, vframe->data, vframe->linesize);

    vnext_pts += delta;
    vframe->pts = vnext_pts;

    /* encode the image */
    // write video frame
    ret = avcodec_send_frame(vc, vframe);
    if (ret < 0) {
        fprintf(stderr, "Error encoding frame\n");
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
          av_packet_rescale_ts(&vpkt, vc->time_base, video_st->time_base);
          vpkt.stream_index = video_st->index;
          /* Write the compressed frame to the media file. */
          ret = av_interleaved_write_frame(oc, &vpkt);
          if (ret < 0) {
              fprintf(stderr, "Error while writing video frame\n");
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

  avio_close(oc->pb);
  avformat_free_context(oc);
}

