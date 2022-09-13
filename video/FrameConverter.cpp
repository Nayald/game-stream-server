#include <unistd.h>
#include <iostream>

#include "FrameConverter.h"
#include "../exception.h"

FrameConverter::FrameConverter() : name("video frame converter"), queue(4) {

}

FrameConverter::~FrameConverter() {
    stop();
}

/*void FrameConverter::init(const std::unordered_map<std::string, std::string> &params) {

}*/

void FrameConverter::init(AVCodecContext *source_ctx, AVCodecContext *sink_ctx, int concurrency) {
    for (auto& [context, frame] : contexts) {
        sws_freeContext(context);
        av_frame_free(&frame);
    }
    contexts.clear();

    for (int i = 0; i < concurrency; ++i) {
        SwsContext *context = sws_getContext(source_ctx->width, source_ctx->height, source_ctx->pix_fmt, sink_ctx->width, sink_ctx->height, sink_ctx->pix_fmt, SWS_AREA, NULL, NULL, NULL);
        AVFrame *frame = av_frame_alloc();
        frame->format = sink_ctx->pix_fmt;
        frame->width  = sink_ctx->width;
        frame->height = sink_ctx->height;
        /*frame->linesize[0] = av_image_get_linesize(sink_ctx->pix_fmt, sink_ctx->width, 0);
        frame->linesize[1] = av_image_get_linesize(sink_ctx->pix_fmt, sink_ctx->width, 1);
        frame->linesize[2] = av_image_get_linesize(sink_ctx->pix_fmt, sink_ctx->width, 2);*/
        contexts.emplace_back(context, frame);
    }

    //buffer_pool = av_buffer_pool_init(av_image_get_buffer_size(sink_ctx->pix_fmt, sink_ctx->width, sink_ctx->height, 0), NULL); // example YUV420 = 12 * w * h
    initialized = true;
    std::cout << name << ": init successfully" << std::endl;
}

void FrameConverter::start() {
    stop_condition = false;
    for (size_t i = 0; i < contexts.size(); ++i) {
        threads.emplace_back(&FrameConverter::run, this, i);
    }
}

void FrameConverter::run(size_t i) {
    AVFrame* frame_in = nullptr;
    AVFrame* frame_out = av_frame_alloc();
    while (!stop_condition && initialized) {
        if (!queue.wait_dequeue_timed(frame_in, std::chrono::milliseconds(100))) {
            continue;
        }

        av_frame_copy_props(frame_out, frame_in);
        frame_out->format = contexts[i].second->format;
        frame_out->width = contexts[i].second->width;
        frame_out->height = contexts[i].second->height;
        if (av_frame_get_buffer(frame_out, 0) < 0) {
            std::cerr << "error" << std::endl;
        }

        /*frame_out->buf[0] = av_buffer_pool_get(buffer_pool);
        frame_out->linesize[0] = contexts[i].second->linesize[0];
        frame_out->linesize[1] = contexts[i].second->linesize[1];
        frame_out->linesize[2] = contexts[i].second->linesize[2];
        frame_out->data[0] = frame_out->buf[0]->data;
        frame_out->data[1] = frame_out->data[0] + 2088992;
        frame_out->data[2] = frame_out->data[1] + 522272;
        *frame_out->extended_data = frame_out->buf[0]->data;*/

        sws_scale(contexts[i].first, frame_in->data, frame_in->linesize, 0, frame_in->height, frame_out->data, frame_out->linesize);
        forward(frame_out);
        av_frame_unref(frame_out);
        av_frame_free(&frame_in);
    }

    av_frame_free(&frame_out);
}

void FrameConverter::stop() {
    if (!stop_condition) {
        for (auto& thread : threads) {
            thread.join();
        }
    }
}

void FrameConverter::handle(AVFrame *frame) {
    if (!queue.try_enqueue(frame)) {
        std::cout << name << ": queue is full" << std::endl;
        av_frame_free(&frame);
    }
}
