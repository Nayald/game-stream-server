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

    bool stop_condition = true;
    std::thread thread;
    moodycamel::BlockingReaderWriterCircularBuffer<AVPacket*> queue;

public:
    RTPVideoSender();
    ~RTPVideoSender() override;

    std::string init(const char *url, AVCodecContext *codec_ctx);
    std::string generateSdp();

    void start();
    void run();
    void stop();

    void handle(AVPacket *packet) override;

    void flush();
};

#endif //REMOTE_DESKTOP_RTPVIDEOSENDER_H
