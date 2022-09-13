#ifndef REMOTE_DESKTOP_OPUSENCODER_H
#define REMOTE_DESKTOP_OPUSENCODER_H

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/avfilter.h>
#include <libavdevice/avdevice.h>
#include <libavutil/audio_fifo.h>
#include <libswscale/swscale.h>
};

#include "../Encoder.h"
#include "../spinlock.h"

class OpusEncoder : public Encoder {
private:
    AVAudioFifo *fifo;
    spinlock lock;
    std::condition_variable cv;

public:
    explicit OpusEncoder();
    ~OpusEncoder() override;

    void init(const std::unordered_map<std::string, std::string> &params) override;

    void runFeed() override;
    void feedImpl(AVFrame *frame);

    void handle(AVFrame *frame) override;
};


#endif //REMOTE_DESKTOP_OPUSENCODER_H
