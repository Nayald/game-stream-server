#ifndef REMOTE_DESKTOP_FRAMECONVERTER_H
#define REMOTE_DESKTOP_FRAMECONVERTER_H

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/avfilter.h>
#include <libavdevice/avdevice.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
};

#include <thread>
#include <atomic>
#include <vector>
#include <condition_variable>
#include <unordered_map>

#include "../concurrentqueue/blockingconcurrentqueue.h"

#include "../Source.h"
#include "../Sink.h"

class FrameConverter : public Sink<AVFrame>, public Source<AVFrame> {
private:
    std::string name;
    bool initialized = false;

    bool stop_condition = true;
    std::vector<std::thread> threads;
    std::vector<std::pair<SwsContext*, AVFrame*>> contexts;
    //AVBufferPool *buffer_pool = nullptr;
    moodycamel::BlockingConcurrentQueue<AVFrame*> queue;

public:
    FrameConverter();
    ~FrameConverter() override;

    //void init(const std::unordered_map<std::string, std::string> &params);
    void init(AVCodecContext *source_ctx, AVCodecContext *sink_ctx, int concurrency=1);

    void start();
    void run(size_t i);
    void stop();

    void handle(AVFrame *frame) override;
};


#endif //REMOTE_DESKTOP_FRAMECONVERTER_H
