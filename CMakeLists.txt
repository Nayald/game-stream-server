cmake_minimum_required(VERSION 3.19)
project(remote_desktop)

set(CMAKE_CXX_STANDARD 17)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
#set(CMAKE_CXX_FLAGS_RELEASE "-O2")
#set(CMAKE_CXX_FLAGS "-O2")

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBAV REQUIRED IMPORTED_TARGET
        libavdevice
        libavfilter
        libavformat
        libavcodec
        libswresample
        libswscale
        libavutil
        )

find_package(Threads REQUIRED)

add_executable(remote_desktop
        main.cpp
        source.cpp Source.h Sink.h exception.h
        Grabber.cpp Grabber.h video/X11Grabber.cpp video/X11Grabber.h audio/AlsaGrabber.cpp audio/AlsaGrabber.h
        Encoder.cpp Encoder.h video/H264Encoder.cpp video/H264Encoder.h audio/OpusEncoder.cpp audio/OpusEncoder.h
        video/FrameConverter.cpp video/FrameConverter.h

        simdjson/singleheader/simdjson.cpp simdjson/singleheader/simdjson.h
        network/socket_server.cpp network/socket_server.h
        network/remote_session.cpp network/remote_session.h
        network/RTPAudioSender.cpp network/RTPAudioSender.h network/RTPVideoSender.cpp network/RTPVideoSender.h

        input/virtual_keyboard.cpp input/virtual_keyboard.h
        input/virtual_mouse.cpp input/virtual_mouse.h
        input/virtual_gamepad.cpp input/virtual_gamepad.h
        )

target_link_libraries(remote_desktop PkgConfig::LIBAV Threads::Threads)
