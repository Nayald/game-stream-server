#ifndef REMOTE_DESKTOP_SOURCE_H
#define REMOTE_DESKTOP_SOURCE_H

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/avfilter.h>
#include <libavdevice/avdevice.h>
#include <libswscale/swscale.h>
};

#include <unordered_set>
#include <memory>

#include "Sink.h"
#include "spinlock.h"

template<class T>
class Source {
private:
    std::unordered_set<Sink<T>*> sinks;
    spinlock lock;

protected:
    Source() = default;
    virtual ~Source() = default;

public:
    void attachSink(Sink<T> *sink) {
        lock.lock();
        sinks.emplace(sink);
        lock.unlock();
    }

    void detachSink(Sink<T> *sink) {
        lock.lock();
        sinks.erase(sink);
        lock.unlock();
    }

protected:
    void forward(T *t) {
        lock.lock();
        for (auto& sink : sinks) {
            sink->handle(t);
        }
        lock.unlock();
    }
};

#endif //REMOTE_DESKTOP_SOURCE_H
