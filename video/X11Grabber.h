#ifndef REMOTE_DESKTOP_X11GRABBER_H
#define REMOTE_DESKTOP_X11GRABBER_H

#include <atomic>
#include <thread>
#include <vector>
#include "../Grabber.h"
#include "../Source.h"

class X11Grabber : public Grabber {
public:
    explicit X11Grabber();
    ~X11Grabber() override = default;

    void init(std::unordered_map<std::string, std::string> &params) override;
};


#endif //REMOTE_DESKTOP_X11GRABBER_H
