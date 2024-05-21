# game-stream-server

The server part of a pair of software intended for playing games on a remote computer and works in a similar way to a remote desktop protocol (RDP). A client connects to the server and performs actions on it remotely. The client receives audio and video streams in return.

The server uses the x11grab input device available in FFmpeg and so can only work with (borderless) windowed games/apps.

The server uses the 9999 TCP and UDP ports for command and inputs and 10000 to 10003 UDP ports for RTP/RTCP

# Overview

![CG server](https://github.com/Nayald/game-stream-server/blob/main/image/CG_Server_BM.png?raw=true)

Most of the work is done with the FFmpeg API for both audio and video stream, the structure follow a pipeline design which each blocks perform a single task.
* For video stream, we start by recording the X11 windowing system at a given sampling rate, equal to the final framerate used for the video encoder (VideoGrabber). An intermediate processing (frameConverter) may be used to adapt the generated frames to the format expected by the encoder block (VideoEncoder). At the end, the encoded frames will be sent to the client via the RTP protocol (RTPVideoSender). Our encoder is set with H264 but can seemlessly uses H265 instead without change (just change the codec string to "hevc_nvenc"/"libx265") as the parameters used are the same for both. Other encoders can be choosen as long as RTP knows a way to packetize them.
* For audio stream, we capture directly the ALSA device of the system (AudioGrabber). The audio frames are given to the AudioEncoder (opus in our case) without futher processing as the encoder. We specify we want low latency, 10ms frames and some inband FEC in case of network losses.
* Virtual peripherals are based on the uinput interface of the system. They enter a bit in conflict with the AudioGrabber because they need elevated rights when the other prohibit it so you need to either change user rights on /dev/uinput (the one we choose) or use for example a different grabber like pulse. For each client, a set of 3 peripherical is created: a keyboard, a mouse and a controller.

Each time a client is connected, the socket server will spaw a RemoteSession object that will manage a TCP socket for commands and a UDP socket for inputs and will forward encoded audio/video frames to FFmpeg (4 others UDP sockets are handled by FFmpeg for the RTP/RTCP streams). The command socket is used by the server to notify to the client the SDP for each streams and the input socket will contains all the axis and button events that will be routed to their respective virtual periphericals. The command socket could be use for future notification, even from the client like to request a sepcific encoder, resolution or framerate, etc.

## Dependencies

FFmpeg 4.4.2 dev libs:
* libavdevice
* libavfilter
* libavformat
* libavcodec
* libswresample
* libswscale
* libavutil

## Installation

On Ubuntu 22.04
* sudo apt install cmake ffmpeg libavdevice-dev libx11-dev libsdl2-dev
* git clone --recurse-submodules https://github.com/Nayald/game-stream-server.git
* cd game-stream-server
* cmake CMakeLists.txt
* make all
