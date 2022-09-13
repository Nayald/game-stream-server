#include <iostream>
#include <utility>

#include "AlsaGrabber.h"
#include "../exception.h"

AlsaGrabber::AlsaGrabber() : Grabber("alsa grabber") {

}

void AlsaGrabber::init(std::unordered_map<std::string, std::string> &params) {
    // re-init check, free old context
    if (codec_ctx) {
        avcodec_free_context(&codec_ctx);
    }

    if (format_ctx) {
        avformat_free_context(format_ctx);
        format_ctx = nullptr;
    }

    // set options
    AVDictionary *options = nullptr;
    for (const auto& [key, val] : params) {
        av_dict_set(&options, key.c_str(), val.c_str(),0);
    }

    format_ctx = avformat_alloc_context();
    AVInputFormat *ifmt = av_find_input_format("alsa");
    //av_dict_set(&options, "sample_rate", "44100", 0);
    if(avformat_open_input(&format_ctx, "default", ifmt, &options) != 0) {
        throw InitFail("Couldn't open input stream");
    }

    if(avformat_find_stream_info(format_ctx,NULL) < 0) {
        throw InitFail("Couldn't find stream information");
    }

    //print info of stream
    av_dump_format(format_ctx, 0, NULL, 0);

    AVCodec *codec;
    stream_index = av_find_best_stream(format_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0);
    if(stream_index < 0) {
        throw InitFail("Couldn't find a video stream");
    }

    codec_ctx = avcodec_alloc_context3(codec);
    int ret = avcodec_parameters_to_context(codec_ctx, format_ctx->streams[stream_index]->codecpar);
    if (ret < 0) {
        throw InitFail("Could not allocate video codec context");
    }

    if(avcodec_open2(codec_ctx, codec, NULL) < 0) {
        throw InitFail("Could not open codec");
    }

    initialized = true;
    std::cerr << name << ": initialized" << std::endl;
    av_dict_free(&options);
}
