#!/bin/sh -xe

ffmpeg -y -framerate 60 -pix_fmt rgb24 -s 512x512 -i frames/frame%03d.raw -c:v libx264 -r 30 -pix_fmt yuv420p -vf vflip frames/movie.mp4

mplayer frames/movie.mp4
