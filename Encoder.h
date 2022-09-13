#ifndef REMOTE_DESKTOP_ENCODER_H
#define REMOTE_DESKTOP_ENCODER_H

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/avfilter.h>
#include <libavdevice/avdevice.h>
#include <libswscale/swscale.h>
};

#include <unordered_map>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>

#include "Sink.h"
#include "Source.h"

class Encoder : public Sink<AVFrame>, public Source<AVPacket> {
protected:
    std::string name;
    bool initialized = false;

    AVCodecContext *codec_ctx = nullptr;
    int64_t frame_id = 0;

    bool feed_stop_condition = true;
    std::thread feed_thread;
    bool drain_stop_condition = true;
    std::thread drain_thread;
    spinlock encoder_lock;
    std::condition_variable encoder_cv;

    explicit Encoder(std::string name);
    ~Encoder() override;

    virtual void runFeed() = 0;
    void runDrain();

public:
    virtual void init(const std::unordered_map<std::string, std::string> &params) = 0;
    AVCodecContext* getContext() const;

    void start();
    void stop();
    void startFeed();
    void stopFeed();
    void startDrain();
    void stopDrain();

    void handle(AVFrame *frame) override = 0;

    void flush();
};


#endif //REMOTE_DESKTOP_ENCODER_H
