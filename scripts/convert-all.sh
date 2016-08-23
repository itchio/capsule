#!/bin/sh -xe

if [ -z "$FPS" ]; then
  FPS=30
fi

if [ -z "$PIXFMT" ]; then
  PIXFMT=rgb24
fi

if [ -z "$PRESET" ]; then
  PRESET=ultrafast
fi

if [ -z "$CRF" ]; then
  CRF=23
fi

if [ -z "$TRANSFORM" ]; then
  TRANSFORM=vflip,scale=iw*.5;-1
fi

ffmpeg -y -f rawvideo -framerate $FPS -pix_fmt $PIXFMT -s "$2" -i "$1" -c:v libx264 -preset $PRESET -crf $CRF -r $FPS -pix_fmt yuv420p -vf $TRANSFORM movie.mp4
mpv movie.mp4 || open movie.mp4
