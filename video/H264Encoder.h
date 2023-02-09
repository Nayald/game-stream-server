#ifndef REMOTE_DESKTOP_H264ENCODER_H
#define REMOTE_DESKTOP_H264ENCODER_H

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/avfilter.h>
#include <libavdevice/avdevice.h>
#include <libswscale/swscale.h>
};

#include <vector>

#include "../readerwriterqueue/readerwritercircularbuffer.h"

#include "../Encoder.h"
#include "../Sink.h"
#include "../spinlock.h"

class H264Encoder : public Encoder, public Sink<const int64_t> {
private:
    bool use_nvenc;

    moodycamel::BlockingReaderWriterCircularBuffer<AVFrame*> queue;
    std::vector<int64_t> bitrate_requests;
    spinlock request_lock;

public:
    explicit H264Encoder(bool use_nvenc=false);
    ~H264Encoder() override = default;

    void init(const std::unordered_map<std::string, std::string> &params) override;

    void handle(AVFrame *frame) override;
    void handle(const int64_t *bitrate_request) override;

private:
    void runFeed() override;
    void feedImpl(AVFrame *frame);
};


#endif //REMOTE_DESKTOP_H264ENCODER_H
