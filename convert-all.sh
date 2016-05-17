#!/bin/sh -xe

ffmpeg -y -f rawvideo -framerate 60 -pix_fmt rgb24 -s 512x512 -i "$1" -c:v libx264 -r 30 -pix_fmt yuv420p -vf vflip movie.mp4
mplayer movie.mp4
