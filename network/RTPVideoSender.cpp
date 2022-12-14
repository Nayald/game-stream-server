#include <string>
#include <iostream>

#include "RTPVideoSender.h"
#include "../exception.h"

RTPVideoSender::RTPVideoSender() : name("rtp video sender"), queue(4) {

}

RTPVideoSender::~RTPVideoSender() {
    std::cout << name << ": next lines are triggered by ~RTPVideoSender() call" << std::endl;
    stop();
    avformat_free_context(format_ctx);
}

void RTPVideoSender::init(const char *url, AVCodecContext *codec_ctx, const char *type) {
    format = av_guess_format(type, url, NULL);
    if (!format) {
        throw InitFail("could not guess format");
    }
    //std::cout << format->name << std::endl;

    format_ctx = avformat_alloc_context();
    if (!format_ctx) {
        throw InitFail("fail to allocate format context");
    }

    int ret = avformat_alloc_output_context2(&format_ctx, format, format->name, url);
    if (ret < 0) {
        throw InitFail("fail to allocate format context");
    }

    AVCodec *codec = avcodec_find_encoder(codec_ctx->codec_id);
    if (!codec) {
        throw InitFail("codec not found");
    }

    stream = avformat_new_stream(format_ctx, codec);
    if (!stream) {
        throw InitFail("could not allocate stream");
    }

    stream->index = format_ctx->nb_streams - 1;
    src_timebase = codec_ctx->time_base;
    avcodec_parameters_from_context(stream->codecpar, codec_ctx);

    AVDictionary *options = NULL;
    //av_dict_set(&options, "fec", "prompeg=l=4:d=4", 0);
    //av_dict_set(&options, "l", "4", 0);
    //av_dict_set(&options, "d", "4", 0);
    if (avformat_init_output(format_ctx, &options) < 0) {
        throw InitFail("error occurred when init output file");
    }

    /* open the output file, if needed */
    if (!(format_ctx->flags & AVFMT_NOFILE) && avio_open(&format_ctx->pb, url, AVIO_FLAG_WRITE) < 0) {
        throw InitFail("error while opening socket");
    }

    if (avformat_write_header(format_ctx, NULL) < 0) {
        throw InitFail("error occurred when opening output file");
    }

    av_dump_format(format_ctx, 0, url, 1);
    initialized = true;
}

std::string RTPVideoSender::generateSdp() {
    char buffer[1024];
    if(av_sdp_create(&format_ctx, 1, buffer, sizeof(buffer)) < 0) {
        throw InitFail("fail to generate sdp");
    }

    return buffer;
}

void RTPVideoSender::start() {
    if (initialized && stop_condition.load(std::memory_order_relaxed)) {
        stop_condition.store(false, std::memory_order_relaxed);
        thread = std::thread(&RTPVideoSender::run, this);
    } else {
        std::cout << name << ": not initialized or thread already running" << std::endl;
    }
}

void RTPVideoSender::stop() {
    if (!stop_condition.load(std::memory_order_relaxed)) {
        stop_condition.store(true, std::memory_order_relaxed);
        if (thread.joinable()) {
            thread.join();
        } else {
            std::cout << name << ": thread is not joinable" << std::endl;
        }
    } else {
        std::cout << name << ": thread is not running" << std::endl;
    }
}

void RTPVideoSender::run() {
    AVPacket *packet = nullptr;
    try {
        while (!stop_condition.load(std::memory_order_relaxed)) {
            if (!queue.wait_dequeue_timed(packet, std::chrono::milliseconds(100))) {
                continue;
            }

            av_packet_rescale_ts(packet, src_timebase, stream->time_base);
            if (av_interleaved_write_frame(format_ctx, packet) < 0) {
                throw InitFail("Error while writing");
            }

            av_packet_free(&packet);
        }
    } catch (const std::exception &e) {
        std::cout << e.what() << std::endl;
    }
}

void RTPVideoSender::handle(AVPacket *packet) {
    if (!queue.try_enqueue(packet)) {
        std::cout << name << ": queue is full" << std::endl;
        av_packet_free(&packet);
    }
}

void RTPVideoSender::flush() {
    av_write_trailer(format_ctx);
    avio_closep(&format_ctx->pb);
    initialized = false;
}
