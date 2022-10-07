#ifndef REMOTE_DESKTOP_RTPVIDEOSENDER_H
#define REMOTE_DESKTOP_RTPVIDEOSENDER_H

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/avfilter.h>
#include <libavdevice/avdevice.h>
#include <libswscale/swscale.h>
};

#include <exception>
#include <thread>
#include <atomic>
#include <condition_variable>

#include "../readerwriterqueue/readerwritercircularbuffer.h"

#include "../Sink.h"

class RTPVideoSender : public Sink<AVPacket> {
private:
    std::string name;
    bool initialized = false;

    AVOutputFormat *format = nullptr;
    AVFormatContext *format_ctx = nullptr;
    AVStream *stream = nullptr;
    AVRational src_timebase;

    std::atomic<bool> stop_condition = true;
    std::thread thread;
    moodycamel::BlockingReaderWriterCircularBuffer<AVPacket*> queue;

public:
    RTPVideoSender();
    ~RTPVideoSender() override;

    void init(const char *url, AVCodecContext *codec_ctx, const char *type="rtp");
    std::string generateSdp();

    void start();
    void stop();

    void handle(AVPacket *packet) override;

private:
    void run();

    void flush();
};

#endif //REMOTE_DESKTOP_RTPVIDEOSENDER_H
