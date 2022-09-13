#include "Source.h"

template <>
void Source<AVFrame>::forward(AVFrame *frame) {
    lock.lock();
    for (auto& sink : sinks) {
        sink->handle(av_frame_clone(frame));
    }
    lock.unlock();
}

template <>
void Source<AVPacket>::forward(AVPacket *packet) {
    lock.lock();
    for (auto& sink : sinks) {
        sink->handle(av_packet_clone(packet));
    }
    lock.unlock();
}
