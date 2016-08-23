#!/bin/sh -xe

FPS?=30
ffmpeg -y -f rawvideo -framerate $FPS -pix_fmt rgb24 -s "$2" -i "$1" -c:v libx264 -preset ultrafast -r $FPS -pix_fmt yuv420p -vf vflip movie.mp4
mpv movie.mp4
