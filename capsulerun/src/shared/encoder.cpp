
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

  // assuming RGB
  const int buffer_size = width * height * 3;
  uint8_t *buffer = (uint8_t*) malloc(buffer_size);
  if (!buffer) {
    printf("could not allocate buffer\n");
    exit(1);
  }

  int components = 3;
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

  AVCodecID codec_id = AV_CODEC_ID_H264;

  AVCodec *codec = avcodec_find_encoder(codec_id);
  if (!codec) {
    printf("could not find codec\n");
    exit(1);
  }

  printf("found codec\n");

  AVCodecContext *c = avcodec_alloc_context3(codec);
  if (!c) {
      printf("could not allocate codec context\n");
      exit(1);
  }

  c->codec_id = codec_id;
  c->codec_type = AVMEDIA_TYPE_VIDEO;
  c->pix_fmt = AV_PIX_FMT_YUV420P;

  // sample parameters
  c->bit_rate = 4000000;
  // resolution must be a multiple of two
  c->width = width;
  c->height = height;
  // frames per second
  video_st->time_base = AVRational{1,25};
  c->time_base = video_st->time_base;

  c->gop_size = 120;
  c->max_b_frames = 16;
  c->rc_buffer_size = 0;

  // H264
  c->qmin = 10;
  c->qmax = 51;

  c->flags |= CODEC_FLAG_GLOBAL_HEADER;

  // av_opt_set(c->priv_data, "preset", "placebo", AV_OPT_SEARCH_CHILDREN);
  av_opt_set(c->priv_data, "preset", "ultrafast", AV_OPT_SEARCH_CHILDREN);

  ret = avcodec_open2(c, codec, NULL);
  if (ret < 0) {
    printf("could not open codec\n");
    exit(1);
  }

  ret = avcodec_parameters_from_context(video_st->codecpar, c);
  if (ret < 0) {
    printf("could not copy codec parameters\n");
    exit(1);
  }

  AVFrame *frame = av_frame_alloc();
  if (!frame) {
    printf("could not allocate frame\n");
  }
  frame->format = c->pix_fmt;
  frame->width = c->width;
  frame->height = c->height;

  // initialize swscale context
  struct SwsContext *sws = sws_getContext(
    // input
    frame->width, frame->height, AV_PIX_FMT_RGB24,
    // output
    frame->width, frame->height, c->pix_fmt,
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
  ret = av_image_alloc(frame->data, frame->linesize, c->width, c->height,
                       c->pix_fmt, 32);
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

  frame->pts = 0;
  int next_pts = 1;

  while (true) {
    size_t read = params->receive_frame(params->private_data, buffer, buffer_size);
    total_read += read;

    if (read < buffer_size) {
      printf("received last frame\n");
      break;
    }

    sws_scale(sws, sws_in, sws_linesize, 0, c->height, frame->data, frame->linesize);

    frame->pts = next_pts++;

    /* encode the image */
    // write video frame
    ret = avcodec_send_frame(c, frame);
    if (ret < 0) {
        fprintf(stderr, "Error encoding frame\n");
        exit(1);
    }

    while (ret >= 0) {
      AVPacket pkt;
      av_init_packet(&pkt);
      ret = avcodec_receive_packet(c, &pkt);

      if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
          fprintf(stderr, "Error encoding a video frame\n");
          exit(1);
      } else if (ret >= 0) {
          av_packet_rescale_ts(&pkt, c->time_base, video_st->time_base);
          pkt.stream_index = video_st->index;
          /* Write the compressed frame to the media file. */
          ret = av_interleaved_write_frame(oc, &pkt);
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

  avcodec_close(c);
  av_freep(&frame->data[0]);
  av_frame_free(&frame);

  avio_close(oc->pb);
  avformat_free_context(oc);
}

