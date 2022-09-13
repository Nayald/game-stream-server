#ifndef REMOTE_DESKTOP_ALSAGRABBER_H
#define REMOTE_DESKTOP_ALSAGRABBER_H

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/avfilter.h>
#include <libavdevice/avdevice.h>
};

#include "../Grabber.h"

class AlsaGrabber : public Grabber {
public:
    explicit AlsaGrabber();
    ~AlsaGrabber() override = default;

    void init(std::unordered_map<std::string, std::string> &params) override;
};


#endif //REMOTE_DESKTOP_ALSAGRABBER_H
