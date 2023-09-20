# game-stream-server
The server part of a pair of software intended for playing games on a remote computer

## dependencies
FFmpeg 4.4.2 dev libs:
* libavdevice
* libavfilter
* libavformat
* libavcodec
* libswresample
* libswscale
* libavutil

## installation
On Ubuntu 22.04
* sudo apt install cmake ffmpeg libavdevice-dev libx11-dev libsdl2-dev
* git clone --recurse-submodules https://github.com/Nayald/game-stream-server.git
* cd game-stream-server
* cmake CMakeLists.txt
* make all
