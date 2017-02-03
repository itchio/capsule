
#include "capsulerun.h"

extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libavutil/mathematics.h>
    #include <libavutil/imgutils.h>
    #include <libswscale/swscale.h>
}

void *encoder_func(void *p) {
  int ret;

  AVCodec *codec;
  AVCodecContext *c = NULL;

  AVFrame *frame;
  AVPacket pkt;
  uint8_t endcode[] = { 0, 0, 1, 0xb7 };

  encoder_params_s *params = (encoder_params_s*) p;

  avcodec_register_all();
  av_register_all();

  av_log_set_level(AV_LOG_DEBUG);

  codec = avcodec_find_encoder(CODEC_ID_H264);
  if (!codec) {
    printf("could not find codec H264\n");
  }

  printf("loaded codec\n");

  // open fifo
  FILE *fifo_file = fopen(params->fifo_path, "rb");
  if (fifo_file == NULL) {
    printf("could not open fifo for reading: %s\n", strerror(errno));
    exit(1);
  }

  printf("opened fifo\n");

  int64_t width, height;
  fread(&width, sizeof(int64_t), 1, fifo_file);
  fread(&height, sizeof(int64_t), 1, fifo_file);

  printf("resolution: %ldx%ld\n", width, height);

  // assuming RGB
  const int BUFFER_SIZE = width * height * 3;
  uint8_t *buffer = (uint8_t*) malloc(BUFFER_SIZE);
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

  fmt = oc->oformat;

  video_st = avformat_new_stream(oc, codec);
  if (!video_st) {
      printf("could not allocate stream\n");
      exit(1);
  }
  video_st->id = oc->nb_streams - 1;
  c = video_st->codec;
  c->coder_type = AVMEDIA_TYPE_VIDEO;

  // sample parameters
  c->bit_rate = 400000;
  // resolution must be a multiple of two
  c->width = width;
  c->height = height;
  // frames per second
  c->time_base = (AVRational){1,25};

  // emit one intra frame every ten frames
  // check frame pict_type before passing frame
  // to encoder, if frame->pict_type is AV_PICTURE_TYPE_I
  // then gop_size is ignored and the output of encoder
  // will always be I frame irrespective to gop_size
  c->gop_size = 10;
  c->max_b_frames = 1;
  c->pix_fmt = AV_PIX_FMT_YUV420P;

  c->flags |= CODEC_FLAG_GLOBAL_HEADER;

  if (avcodec_open2(c, codec, NULL) < 0) {
    printf("could not open codec\n");
    exit(1);
  }

  frame = av_frame_alloc();
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

  /* open the output file, if needed */
  ret = avio_open(&oc->pb, output_path, AVIO_FLAG_WRITE);
  if (ret < 0) {
      fprintf(stderr, "Could not open '%s'\n", output_path);
      exit(1);
  }

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

  while (true) {
    size_t read = fread(buffer, 1, BUFFER_SIZE, fifo_file);
    total_read += read;

    if (read < BUFFER_SIZE) {
      printf("Stopped reading\n");
      break;
    }

    av_init_packet(&pkt);
    pkt.data = NULL;
    pkt.size = 0;

    sws_scale(sws, sws_in, sws_linesize, 0, c->height, frame->data, frame->linesize);

    /* encode the image */
    ret = avcodec_encode_video2(c, &pkt, frame, &got_output);
    if (ret < 0) {
        fprintf(stderr, "Error encoding frame\n");
        exit(1);
    }

    if (got_output && pkt.size) {
        pkt.stream_index = video_st->index;

        // printf("Write frame %3d (size=%5d)\n", i, pkt.size);
        ret = av_interleaved_write_frame(oc, &pkt);
        if (ret < 0) {
            fprintf(stderr, "Error muxing frame\n");
            exit(1);
        }
    }

    // if ((total_read - last_print_read) > 1024 * 1024) {
    //   last_print_read = total_read;
    //   printf("read %.2f MB\n", ((double)total_read) / 1024.0 / 1024.0 );
    // }

    // write video frame
    frame->pts += av_rescale_q(1, video_st->codec->time_base, video_st->time_base);
  }

  int i;

  /* get the delayed frames */
  for (got_output = 1; got_output; i++) {
    ret = avcodec_encode_video2(c, &pkt, NULL, &got_output);
    if (ret < 0) {
      fprintf(stderr, "Error encoding frame\n");
      exit(1);
    }
 
    if (got_output && pkt.size) {
      pkt.stream_index = video_st->index;

      printf("Write delayed frame %3d (size=%5d)\n", i, pkt.size);
      ret = av_interleaved_write_frame(oc, &pkt);
      if (ret < 0) {
          fprintf(stderr, "Error muxing frame\n");
          exit(1);
      }
    }
  }

  // Write format trailer if any
  ret = av_write_trailer(oc);
  if (ret < 0) {
    printf("failed to write trailer\n");
    exit(1);
  }

  avcodec_free_context(&c);
  av_freep(&frame->data[0]);
  av_frame_free(&frame);

  avformat_free_context(oc);

  return NULL;
}

