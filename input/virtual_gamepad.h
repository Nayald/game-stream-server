#ifndef REMOTE_DESKTOP_VIRTUALGAMEPAD_H
#define REMOTE_DESKTOP_VIRTUALGAMEPAD_H

#include <linux/uinput.h>

#include <unordered_map>
#include <chrono>
#include <thread>
#include <atomic>

#include "../spinlock.h"

class VirtualGamepad {
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
    VirtualGamepad();
    ~VirtualGamepad();

    void init();

    void startRelease();
    void stopRelease();

    void setPositionAbsolute(int x, int y, int rx, int ry, int z, int rz);
    void setButtonStates(int button_states);

private:
    void release();
};


#endif //REMOTE_DESKTOP_VIRTUALGAMEPAD_H
