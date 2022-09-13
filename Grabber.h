#ifndef REMOTE_DESKTOP_GRABBER_H
#define REMOTE_DESKTOP_GRABBER_H

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/avfilter.h>
#include <libavdevice/avdevice.h>
};

#include <exception>
#include <string>
#include <unordered_map>
#include <thread>

#include "Source.h"

class Grabber : public Source<AVPacket>, public Source<AVFrame> {
protected:
    std::string name;
    bool initialized = false;

    AVFormatContext *format_ctx = nullptr;
    int stream_index;
    AVCodecContext *codec_ctx = nullptr;

    bool stop_condition = true;
    std::thread grab_thread;

    explicit Grabber(std::string name);
    ~Grabber() override;

public:
    virtual void init(std::unordered_map<std::string, std::string> &params) = 0;
    AVCodecContext* getContext();

    void start();
    void stop();
    void run();
};

#endif
