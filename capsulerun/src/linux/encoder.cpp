
#include "capsulerun.h"

extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavutil/mathematics.h>
    #include <libavutil/imgutils.h>
}

void *encoder_func(void *p) {
  AVCodec *codec;
  AVCodecContext *c = NULL;

  AVFrame *frame;
  AVPacket pkt;
  uint8_t endcode[] = { 0, 0, 1, 0xb7 };

  encoder_params_s *params = (encoder_params_s*) p;

  avcodec_register_all();

  codec = avcodec_find_encoder(CODEC_ID_H264);
  if (!codec) {
    printf("could not find codec H264\n");
  }

  printf("loaded codec\n");

  c = avcodec_alloc_context3(codec);
  if (!c) {
    fprintf(stderr, "Could not allocate video codec context\n");
    exit(1);
  }

  printf("allocated video codec context\n");

  // open output file
  FILE *output_file = fopen("capsule.h264", "wb");
  if (output_file == NULL) {
    printf("could not open output file for writing: %s\n", strerror(errno));
    exit(1);
  }

  printf("opened output file\n");

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
//   c->pix_fmt = AV_PIX_FMT_RGB24;

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

  int ret;

  /* the image can be allocated by any means and av_image_alloc() is
   * just the most convenient way if av_malloc() is to be used */
  ret = av_image_alloc(frame->data, frame->linesize, c->width, c->height,
                       c->pix_fmt, 32);
  if (ret < 0) {
      fprintf(stderr, "Could not allocate raw picture buffer\n");
      exit(1);
  }

  // assuming RGB
  const int BUFFER_SIZE = width * height * 3;
  char *buffer = (char*) malloc(BUFFER_SIZE);
  if (!buffer) {
    printf("could not allocate buffer\n");
    exit(1);
  }

  size_t total_read = 0;
  size_t last_print_read = 0;

  int components = 3;
  int linesize = width * components;

  int x, y;
  int i;
  int got_output;

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

    for (y = 0; y < c->height; y++) {
      for (x = 0; x < c->width; x++) {
        frame->data[0][y * frame->linesize[0] + x] = buffer[y * linesize + x * components];
      }
    }

    frame->pts = i;

    /* encode the image */
    ret = avcodec_encode_video2(c, &pkt, frame, &got_output);
    if (ret < 0) {
        fprintf(stderr, "Error encoding frame\n");
        exit(1);
    }

    if (got_output) {
        printf("Write frame %3d (size=%5d)\n", i, pkt.size);
        fwrite(pkt.data, 1, pkt.size, output_file);
        av_packet_unref(&pkt);
    }

    if ((total_read - last_print_read) > 1024 * 1024) {
      last_print_read = total_read;
      printf("read %.2f MB\n", ((double)total_read) / 1024.0 / 1024.0 );
    }

    i++;
  }


  /* get the delayed frames */
  for (got_output = 1; got_output; i++) {
    fflush(stdout);
 
    ret = avcodec_encode_video2(c, &pkt, NULL, &got_output);
    if (ret < 0) {
      fprintf(stderr, "Error encoding frame\n");
      exit(1);
    }
 
    if (got_output) {
      printf("Write frame %3d (size=%5d)\n", i, pkt.size);
      fwrite(pkt.data, 1, pkt.size, output_file);
      av_packet_unref(&pkt);
    }
  }

  /* add sequence end code to have a real MPEG file */
  fwrite(endcode, 1, sizeof(endcode), output_file);
  fclose(output_file);
  
  avcodec_free_context(&c);
  av_freep(&frame->data[0]);
  av_frame_free(&frame);

  return NULL;
}

