#include <iostream>
#include <thread>
#include <chrono>
#include <unistd.h>

#include "H264Encoder.h"
#include "../exception.h"

H264Encoder::H264Encoder(bool use_nvenc) : Encoder("h264 encoder"), use_nvenc(use_nvenc), queue(2) {

}

void H264Encoder::init(const std::unordered_map<std::string, std::string> &params) {
    // re-init check, free old context
    if (codec_ctx) {
        avcodec_free_context(&codec_ctx);
    }

    //AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    AVCodec *codec;
    if (use_nvenc) {
        codec = avcodec_find_encoder_by_name("h264_nvenc");
    } else {
        auto it = params.find("pixel_format");
        if (it == params.end()) {
            codec = avcodec_find_encoder(AV_CODEC_ID_H264);
        } else if (it->second == std::to_string(AV_PIX_FMT_BGR0)) {
            codec = avcodec_find_encoder_by_name("libx264rgb");
        } else {
            codec = avcodec_find_encoder_by_name("libx264");
        }
    }

    if (!codec) {
        throw InitFail("Codec not found");
    }

    codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        throw InitFail("Could not allocate video codec context");
    }

    // set options
    AVDictionary *options = nullptr;
    for (const auto& [key, val] : params) {
        if (key == "bitrate") {
            codec_ctx->bit_rate = std::stoi(val);
        } else if (key == "width") {
            codec_ctx->width = std::stoi(val);
        } else if (key == "height") {
            codec_ctx->height = std::stoi(val);
        } else if (key == "pixel_format") {
            codec_ctx->pix_fmt = static_cast<AVPixelFormat>(std::stoi(val));
        } else if (key == "framerate") {
            codec_ctx->time_base = {1, std::stoi(val)};
            codec_ctx->framerate = {std::stoi(val), 1};
        } else if (key == "gop_size") {
            codec_ctx->gop_size = std::stoi(val);
        } else {
            int ret = av_opt_set(codec_ctx->priv_data, key.c_str(), val.c_str(),0);
            if (ret == AVERROR_OPTION_NOT_FOUND) {
                std::cout << name << ": option " << key << " not found" << std::endl;
            } else if (ret == AVERROR(ERANGE) || ret == AVERROR(EINVAL)) {
                std::cout << name << ": value for " << key << " is not valid" << std::endl;
            }
        }
    }

    int ret = avcodec_open2(codec_ctx, codec, &options);
    if (ret < 0) {
        throw InitFail("Could not open codec");
    }

    initialized = true;
    std::cerr << name << ": initialized" << std::endl;
    av_dict_free(&options);
}

void H264Encoder::runFeed() {
    std::cerr << name << ": feed thread pid is " << gettid() << std::endl;
    std::cout << gettid() << std::endl;
    AVFrame* frame;
    try {
        while (initialized && !feed_stop_condition) {
            if (!queue.wait_dequeue_timed(frame, std::chrono::milliseconds(100))) {
                continue;
            }

            feedImpl(frame);
            av_frame_free(&frame);
        }
    } catch (const std::exception &e) {
        std::cout << e.what() << std::endl;
    }
}

void H264Encoder::feedImpl(AVFrame *frame) {
    frame->pts = frame_id;
    frame->pict_type = AV_PICTURE_TYPE_NONE; // usefull if grabber set all to I-frame so encoder does not output only I-frame
    encoder_lock.lock();
    int ret = avcodec_send_frame(codec_ctx, frame);
    encoder_lock.unlock();
    encoder_cv.notify_all();
    if (ret >= 0) {
        ++frame_id;
    } else if (ret == AVERROR(EAGAIN)) {
        std::cout << name << ": encoder buffer may be full, drop frame" << std::endl;
    } else if (ret < 0) {
        throw RunError("error when sending frame to encoder");
    }
}

void H264Encoder::handle(AVFrame *frame) {
    if (feed_thread.joinable()) {
        if (!queue.try_enqueue(frame)) {
            std::cout << name << ": queue is full" << std::endl;
            av_frame_free(&frame);
        }
    // possible to run without feed thread so source will try to handle the job
    } else if (initialized) {
        feedImpl(frame);
        av_frame_free(&frame);
    }
}
