#include <unistd.h>
#include <csignal>
#include <iostream>
#include <thread>

#include "video/X11Grabber.h"
#include "video/FrameConverter.h"
#include "video/H264Encoder.h"
#include "audio/AlsaGrabber.h"
#include "audio/OpusEncoder.h"
#include "network/socket_server.h"


bool stop = false;
void signalHandler( int signum ) {
    std::cout << "Interrupt signal (" << signum << ") received.\n";
    stop = true;
}

int main() {
    signal(SIGINT, signalHandler);
    //signal(SIGTERM, signalHandler);

    printf("libav version: %s\n", av_version_info());
    avdevice_register_all();
    avformat_network_init();

    try {
        //audio chain
        std::unordered_map<std::string, std::string> audio_capture_options = {
                //{"sample_rate", "44100"},
        };
        AlsaGrabber audio_source;
        audio_source.init(audio_capture_options);

        std::unordered_map<std::string, std::string> audio_encoder_options = {
                {"bitrate", "128000"},
                {"sample_rate", std::to_string(audio_source.getContext()->sample_rate)},
                {"sample_format", std::to_string(audio_source.getContext()->sample_fmt)},
                {"channels", std::to_string(audio_source.getContext()->channels)},
                {"application", "lowdelay"},
                {"frame_duration", "10"},
                {"fec", "1"},
                {"packet_loss", "25"},
        };
        OpusEncoder audio_encoder;
        audio_encoder.init(audio_encoder_options);
        audio_source.Source<AVFrame>::attachSink(&audio_encoder);

        audio_encoder.startDrain();
        audio_source.start();

        //video chain
        std::unordered_map<std::string, std::string> video_grabber_options = {
                {"video_size", "2560x1440"},
                {"framerate", "60"},
                {"follow_mouse", "centered"},
                {"probesize", "32M"},
        };
        X11Grabber video_source;
        video_source.init(video_grabber_options);
        video_source.start();

        std::unordered_map<std::string, std::string> video_encoder_options = {
                {"bitrate", "15000000"},
                {"width", "1920"},
                {"height", "1080"},
                {"framerate", "60"},
                {"gop_size", "120"},
                {"pixel_format", std::to_string(AV_PIX_FMT_YUV420P)},
                //{"pixel_format", std::to_string(video_source.getContext()->pix_fmt)}, // nvenc only
                //{"preset", "veryfast"}, // software
                //{"tune", "zerolatency"}, // software
                {"preset", "p4"}, // nvenc
                {"tune", "ull"}, // nvenc
                {"rc", "cbr"}, // nvenc
                {"zerolatency", "1"}, // nvenc
        };
        H264Encoder video_encoder(true);
        video_encoder.init(video_encoder_options);
        video_encoder.startDrain();

        FrameConverter video_converter;
        if (video_source.getContext()->width != video_encoder.getContext()->width ||
            video_source.getContext()->height != video_encoder.getContext()->height ||
            video_source.getContext()->pix_fmt != video_encoder.getContext()->pix_fmt) {
            video_converter.init(video_source.getContext(), video_encoder.getContext(), 2);
            video_converter.start();
            video_converter.attachSink(&video_encoder);
            video_source.Source<AVFrame>::attachSink(&video_converter);
        } else {
            video_source.Source<AVFrame>::attachSink(&video_encoder);
        }

        SocketServer server(audio_encoder, video_encoder);
        server.init();
        server.start();

        while (!stop) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        audio_source.stop();
        audio_encoder.stop();
        video_source.stop();
        video_converter.stop();
        video_encoder.stop();
        server.stop();

    } catch (std::exception &e) {
        std::cout << e.what() << std::endl;
    }
    return 0;
}