#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>

#include "Encoder.h"
#include "exception.h"

Encoder::Encoder(std::string name) : name(std::move(name)) {

}

Encoder::~Encoder() {
    std::cout << name << ": next lines are triggered by ~Encoder() call" << std::endl;
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
    if (initialized && feed_stop_condition.load(std::memory_order_relaxed)) {
        feed_stop_condition.store(false, std::memory_order_relaxed);
        feed_thread = std::thread(&Encoder::runFeed, this);
    } else {
        std::cout << name << ": not initialized or feed thread already running" << std::endl;
    }
}

void Encoder::stopFeed() {
    if (!feed_stop_condition.load(std::memory_order_relaxed)) {
        feed_stop_condition.store(true, std::memory_order_relaxed);
        if (feed_thread.joinable()) {
            feed_thread.join();
        } else {
            std::cout << name << ": feed thread is not joinable" << std::endl;
        }
    } else {
        std::cout << name << ": feed thread is not running" << std::endl;
    }
}

void Encoder::startDrain() {
    if (initialized && drain_stop_condition.load(std::memory_order_relaxed)) {
        drain_stop_condition.store(false, std::memory_order_relaxed);
        drain_thread = std::thread(&Encoder::runDrain, this);
    } else {
        std::cout << name << ": not initialized or drain thread already running" << std::endl;
    }
}

void Encoder::stopDrain() {
    if (!drain_stop_condition.load(std::memory_order_relaxed)) {
        drain_stop_condition.store(true, std::memory_order_relaxed);
        if (drain_thread.joinable()) {
            drain_thread.join();
        } else {
            std::cout << name << ": drain thread is not joinable" << std::endl;
        }
    } else {
        std::cout << name << ": drain thread is not running" << std::endl;
    }
}

void Encoder::runDrain() {
    std::cerr << name << " drain thread pid is " << gettid() << std::endl;
    std::mutex m;
    AVPacket *packet = av_packet_alloc();
    int ret = 0;
    try {
        while (!drain_stop_condition.load(std::memory_order_relaxed)) {
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

void Encoder::flush() {
    if (!feed_stop_condition || !drain_stop_condition) {
        std::cout << name << ": flush order ignored, stop runFeed/runDrain threads first" << std::endl;
        return;
    }

    if (!initialized) {
        std::cout << name << ": not initialized, nothing to do" << std::endl;
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
