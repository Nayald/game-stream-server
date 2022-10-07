#ifndef REMOTE_DESKTOP_VIRTUALMOUSE_H
#define REMOTE_DESKTOP_VIRTUALMOUSE_H

#include <linux/uinput.h>

#include <unordered_map>
#include <chrono>
#include <thread>
#include "../spinlock.h"

class VirtualMouse {
private:
    static std::atomic<size_t> counter;

    std::string name;
    bool initialized = false;

    size_t idx;
    int fd;

    std::atomic<bool> release_stop_condition = true;
    std::thread release_thread;
    std::unordered_map<uint16_t, std::chrono::steady_clock::time_point> pressed_buttons;
    spinlock lock;

public:
    VirtualMouse();
    ~VirtualMouse();

    void init();

    void startRelease();
    void stopRelease();

    void setPositionRelative(int x, int y);
    void setButtonStates(int button_states);
    void setWheelPositionRelative(int x, int y);

private:
    void release();
};


#endif //REMOTE_DESKTOP_VIRTUALMOUSE_H
