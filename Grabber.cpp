#include <iostream>
#include <csignal>

#include "Grabber.h"
#include "exception.h"

Grabber::Grabber(std::string name) : name(std::move(name)) {

}

Grabber::~Grabber() {
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
    if (stop_condition) {
        stop_condition = false;
        grab_thread = std::thread(&Grabber::run, this);
    }
}

void Grabber::stop() {
    if (!stop_condition) {
        stop_condition = true;
        if (grab_thread.joinable()) {
            grab_thread.join();
        }
    }
}

void Grabber::run() {
    std::cerr << name << ": pid is " << gettid() << std::endl;
    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    int ret;
    try {
        while (initialized && !stop_condition) {
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
