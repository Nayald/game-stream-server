#include <iostream>
#include <csignal>

#include "Grabber.h"
#include "exception.h"

Grabber::Grabber(std::string name) : name(std::move(name)) {

}

Grabber::~Grabber() {
    std::cout << name << ": next lines are triggered by ~Grabber() call" << std::endl;
    stop();
    if (codec_ctx) {
        avcodec_free_context(&codec_ctx);
    }

    if (format_ctx) {
        avformat_free_context(format_ctx);
        format_ctx = nullptr;
    }
}

AVCodecContext *Grabber::getContext() {
    return codec_ctx;
}

void Grabber::start() {
    if (initialized && stop_condition.load(std::memory_order_relaxed)) {
        stop_condition.store(false, std::memory_order_relaxed);
        grab_thread = std::thread(&Grabber::run, this);
    } else {
        std::cout << name << ": not initialized or thread already running" << std::endl;
    }
}

void Grabber::stop() {
    if (!stop_condition.load(std::memory_order_relaxed)) {
        stop_condition.store(true, std::memory_order_relaxed);
        if (grab_thread.joinable()) {
            grab_thread.join();
        } else {
            std::cout << name << ": thread is not joinable" << std::endl;
        }
    } else {
        std::cout << name << ": thread is not running" << std::endl;
    }
}

void Grabber::run() {
    std::cerr << name << ": pid is " << gettid() << std::endl;
    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    int ret;
    try {
        while (!stop_condition.load(std::memory_order_relaxed)) {
            if (av_read_frame(format_ctx, packet) < 0) {
                throw RunError("can't grab frame");
            }

            if (packet->stream_index != stream_index) {
                throw RunError("wrong index");
            }

            Source<AVPacket>::forward(packet);

            ret = avcodec_send_packet(codec_ctx, packet);
            if (ret < 0) {
                throw RunError("decode packet error");
            }
            while (ret >= 0) {
                ret = avcodec_receive_frame(codec_ctx, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    break;
                } else if (ret < 0) {
                    throw RunError("error during decoding");
                }

                Source<AVFrame>::forward(frame);
                av_frame_unref(frame);
            }

            av_packet_unref(packet);
        }
    } catch (const std::exception &e) {
        std::cerr << name << ": " << e.what() << std::endl;
    }

    av_frame_free(&frame);
    av_packet_free(&packet);
}
