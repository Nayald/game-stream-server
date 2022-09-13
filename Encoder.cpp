#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>

#include "Encoder.h"
#include "exception.h"

Encoder::Encoder(std::string name) : name(std::move(name)) {

}

Encoder::~Encoder() {
    stop();
    avcodec_free_context(&codec_ctx);
}

AVCodecContext* Encoder::getContext() const {
    return codec_ctx;
}

void Encoder::start() {
    startFeed();
    startDrain();
}

void Encoder::stop() {
    stopFeed();
    stopDrain();
    flush();
}

void Encoder::startFeed() {
    if (feed_stop_condition) {
        feed_stop_condition = false;
        feed_thread = std::thread(&Encoder::runFeed, this);
    }
}

void Encoder::stopFeed() {
    if (!feed_stop_condition) {
        feed_stop_condition = true;
        feed_thread.join();
    }
}

void Encoder::startDrain() {
    if (drain_stop_condition) {
        drain_stop_condition = false;
        drain_thread = std::thread(&Encoder::runDrain, this);
    }
}

void Encoder::runDrain() {
    std::cerr << name << " drain thread pid is " << gettid() << std::endl;
    std::mutex m;
    AVPacket *packet = av_packet_alloc();
    int ret = 0;
    try {
        while (initialized && !drain_stop_condition) {
            encoder_lock.lock();
            ret = avcodec_receive_packet(codec_ctx, packet);
            encoder_lock.unlock();
            if (ret == AVERROR(EAGAIN)) {
                std::unique_lock<std::mutex> mlock(m);
                encoder_cv.wait(mlock, [this, &ret, &packet] {
                    encoder_lock.lock();
                    ret = avcodec_receive_packet(codec_ctx, packet);
                    encoder_lock.unlock();
                    return ret != AVERROR(EAGAIN);
                });
            }

            if (ret < 0) {
                throw RunError("error when receiving packet from encoder");
            } else {
                forward(packet);
                av_packet_unref(packet);
            }
        }
    } catch (const std::exception &e) {
        std::cout << e.what() << std::endl;
    }

    av_packet_free(&packet);
}

void Encoder::stopDrain() {
    if (!drain_stop_condition) {
        drain_stop_condition = true;
        drain_thread.join();
    }
}

void Encoder::flush() {
    if (!feed_stop_condition || !drain_stop_condition) {
        std::cout << name << ": flush order ignored, stop runFeed/runDrain threads first" << std::endl;
        return;
    }

    int ret = avcodec_send_frame(codec_ctx, nullptr);
    AVPacket *packet = av_packet_alloc();
    try {
        while (ret >= 0 || ret == AVERROR(EAGAIN)) {
            ret = avcodec_receive_packet(codec_ctx, packet);
            if (ret < 0 && ret != AVERROR_EOF) {
                throw RunError("error while flushing encoder");
            }

            forward(packet);
            av_packet_unref(packet);
        }
    } catch (const std::exception &e) {
        std::cout << e.what() << std::endl;
    }

    av_packet_free(&packet);
    initialized = false;
}
