#!/bin/sh -xe

ffmpeg -y -f rawvideo -framerate 30 -pix_fmt rgb24 -s "$2" -i "$1" -c:v libx264 -preset ultrafast -r 30 -pix_fmt yuv420p -vf vflip movie.mp4
mpv movie.mp4
