#include <iostream>
#include <unistd.h>

#include "OpusEncoder.h"
#include "../exception.h"

static int check_sample_fmt(const AVCodec *codec, AVSampleFormat sample_fmt) {
    const AVSampleFormat *p = codec->sample_fmts;
    while (*p != AV_SAMPLE_FMT_NONE) {
        if (*p++ == sample_fmt) {
            return 1;
        }
    }
    return 0;
}

OpusEncoder::OpusEncoder() : Encoder("opus encoder") {

}

OpusEncoder::~OpusEncoder() {
    av_audio_fifo_free(fifo);
}

void OpusEncoder::init(const std::unordered_map<std::string, std::string> &params) {
    // re-init check, free old context
    if (codec_ctx) {
        avcodec_free_context(&codec_ctx);
    }

    AVCodec *codec = avcodec_find_encoder_by_name("libopus");
    //codec = avcodec_find_encoder(AV_CODEC_ID_OPUS);
    if (!codec) {
        throw InitFail("codec not found");
    }

    codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        throw InitFail("could not allocate video codec context");
    }

    // set options
    AVDictionary *options = nullptr;
    for (const auto& [key, val] : params) {
        if (key == "bitrate") {
            codec_ctx->bit_rate = std::stoi(val);
        } else if (key == "sample_format") {
            codec_ctx->sample_fmt = static_cast<AVSampleFormat>(std::stoi(val));
            if (!check_sample_fmt(codec, codec_ctx->sample_fmt)) {
                throw InitFail("encoder does not support sample format");
            }
        } else if (key == "sample_rate") {
            codec_ctx->sample_rate = std::stoi(val);
        } else if (key == "channels") {
            codec_ctx->channels = std::stoi(val);
            codec_ctx->channel_layout = av_get_default_channel_layout(codec_ctx->channels);
        } else {
            int ret = av_opt_set(codec_ctx->priv_data, key.c_str(), val.c_str(),0);
            if (ret == AVERROR_OPTION_NOT_FOUND) {
                std::cout << name << ": option " << key << " not found" << std::endl;
            } else if (ret == AVERROR(ERANGE) || ret == AVERROR(EINVAL)) {
                std::cout << name << ": value for " << key << " is not valid" << std::endl;
            }
        }
    }

    if (avcodec_open2(codec_ctx, codec, NULL) < 0) {
        throw InitFail("could not open codec");
    }

    fifo = av_audio_fifo_alloc(codec_ctx->sample_fmt, codec_ctx->channels, 2 * codec_ctx->frame_size);
    if (!fifo) {
        throw RunError("fail to allocate audio buffer");
    }

    initialized = true;
    std::cerr << name << ": initialized" << std::endl;
    av_dict_free(&options);
}

void OpusEncoder::runFeed() {
    std::cout << name << ": " << gettid() << std::endl;
    std::mutex m;
    AVFrame *frame = av_frame_alloc();
    int ret = 0;
    try {
        while (initialized && !feed_stop_condition) {
            lock.lock();
            ret = av_audio_fifo_size(fifo);
            lock.unlock();
            if (ret < codec_ctx->frame_size) {
                // wait for more samples
                std::unique_lock<std::mutex> mlock(m);
                cv.wait(mlock, [this] {
                    lock.lock();
                    int size = av_audio_fifo_size(fifo);
                    lock.unlock();
                    return size >= codec_ctx->frame_size;
                });
            }

            frame->pts = frame_id;
            frame->nb_samples = codec_ctx->frame_size;
            frame->format = codec_ctx->sample_fmt;
            frame->sample_rate = codec_ctx->sample_rate;
            frame->channels = codec_ctx->channels;
            frame->channel_layout = codec_ctx->channel_layout;
            if (av_frame_get_buffer(frame, 0) < 0) {
                throw RunError("can't allocate frame buffer");
            }

            lock.lock();
            if (av_audio_fifo_read(fifo, (void**)frame->data, frame->nb_samples) < frame->nb_samples) {
                throw RunError("buffer was not able to fill the frame");
            }

            lock.unlock();
            feedImpl(frame);
            av_frame_unref(frame);
        }
    } catch (std::exception &e) {
        std::cout << e.what() << std::endl;
    }

    av_frame_free(&frame);
}

void OpusEncoder::feedImpl(AVFrame *frame) {
    encoder_lock.lock();
    int ret = avcodec_send_frame(codec_ctx, frame);
    encoder_lock.unlock();
    encoder_cv.notify_all();
    if (ret >= 0) {
        frame_id += frame->nb_samples;
    /*} if (ret == AVERROR(EAGAIN)) {
        std::cout << name << ": encoder buffer may be full, drop frame" << std::endl;
    */} else if (ret < 0 && ret != AVERROR(EAGAIN)) {
        throw RunError("error when sending frame to encoder");
    }
}

void OpusEncoder::handle(AVFrame *frame) {
    if (feed_thread.joinable()) {
        lock.lock();
        /* Store the new samples in the FIFO buffer, auto reallocate if needed. */
        if (av_audio_fifo_write(fifo, (void**)frame->data, frame->nb_samples) < 0) {
            std::cout << name << ": unable to write data in buffer" << std::endl;
        }

        lock.unlock();
        cv.notify_all();
    } else if (initialized) {
        if (av_audio_fifo_write(fifo, (void**)frame->data, frame->nb_samples) < 0) {
            std::cout << name << ": unable to write data in buffer" << std::endl;
        }

        if (av_audio_fifo_size(fifo) >= codec_ctx->frame_size) {
            av_frame_unref(frame);
            frame->pts = frame_id;
            frame->nb_samples = codec_ctx->frame_size;
            frame->format = codec_ctx->sample_fmt;
            frame->sample_rate = codec_ctx->sample_rate;
            frame->channels = codec_ctx->channels;
            frame->channel_layout = codec_ctx->channel_layout;
            if (av_frame_get_buffer(frame, 0) < 0) {
                throw RunError("can't allocate frame buffer");
            }

            if (av_audio_fifo_read(fifo, (void**)frame->data, frame->nb_samples) < frame->nb_samples) {
                throw RunError("buffer was not able to fill the frame");
            }

            feedImpl(frame);
        }
    }

    av_frame_free(&frame);
}
