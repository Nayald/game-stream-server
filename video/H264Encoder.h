#ifndef REMOTE_DESKTOP_H264ENCODER_H
#define REMOTE_DESKTOP_H264ENCODER_H

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/avfilter.h>
#include <libavdevice/avdevice.h>
#include <libswscale/swscale.h>
};

#include "../readerwriterqueue/readerwritercircularbuffer.h"

#include "../Encoder.h"

class H264Encoder : public Encoder {
private:
    bool use_nvenc;

    moodycamel::BlockingReaderWriterCircularBuffer<AVFrame*> queue;

public:
    explicit H264Encoder(bool use_nvenc=false);
    ~H264Encoder() override = default;

    void init(const std::unordered_map<std::string, std::string> &params) override;

    void handle(AVFrame *frame) override;

private:
    void runFeed() override;
    void feedImpl(AVFrame *frame);
};


#endif //REMOTE_DESKTOP_H264ENCODER_H
