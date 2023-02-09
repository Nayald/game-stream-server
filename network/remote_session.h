#ifndef REMOTE_DESKTOP_REMOTE_SESSION_H
#define REMOTE_DESKTOP_REMOTE_SESSION_H

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/avfilter.h>
#include <libavdevice/avdevice.h>
#include <libswscale/swscale.h>
};

#include <netinet/in.h>
#include <chrono>
#include <unordered_map>

#include "../simdjson/singleheader/simdjson.h"

#include "RTPAudioSender.h"
#include "RTPVideoSender.h"
#include "../Source.h"
#include "../input/virtual_keyboard.h"
#include "../input/virtual_mouse.h"
#include "../input/virtual_gamepad.h"
#include "../spinlock.h"

class RemoteSession : public Source<const int64_t> {
public:
    std::string name;
    bool initialized = false;

    RTPAudioSender rtp_audio;
    RTPVideoSender rtp_video;
    VirtualKeyboard keyboard;
    VirtualMouse mouse;
    //std::unordered_map<int, VirtualGamepad> gamepads;
    VirtualGamepad gamepad;

    sockaddr_in remote_address;
    int tcp_socket;
    int udp_socket;
    spinlock udp_socket_lock;
    simdjson::ondemand::parser parser;

    bool stop_condition = true;
    std::thread thread;

    std::chrono::steady_clock::time_point last_update = std::chrono::steady_clock::now();

public:
    RemoteSession(sockaddr_in remote_address, int tcp_socket);
    ~RemoteSession();

    void init(AVCodecContext *audio_context, AVCodecContext *video_context);
    RTPAudioSender& getRtpAudio();
    RTPVideoSender& getRtpVideo();

    void start();
    void run();
    void stop();

    std::chrono::steady_clock::time_point getLastUpdate() const;

    void write(const std::string &msg);
    void write(const char *msg, size_t size);

private:
    void handleCommands(uint8_t *buffer, size_t size, size_t capacity);
    void handleInputs(uint8_t *buffer, size_t size, size_t capacity);

    void writeImpl(const char *msg, size_t size);
};

#endif //REMOTE_DESKTOP_REMOTE_SESSION_H
