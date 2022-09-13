#ifndef REMOTE_DESKTOP_VIRTUAL_KEYBOARD_H
#define REMOTE_DESKTOP_VIRTUAL_KEYBOARD_H

#include <linux/uinput.h>

#include <unordered_map>
#include <chrono>
#include <thread>
#include <atomic>

#include "../spinlock.h"

class VirtualKeyboard {
private:
    static std::atomic<size_t> counter;

    size_t idx;
    int fd;

    bool release_stop_condition = true;
    std::thread release_thread;
    std::unordered_map<uint16_t, std::chrono::steady_clock::time_point> pressed_keys;
    spinlock lock;

public:
    VirtualKeyboard();
    ~VirtualKeyboard();

    void init();

    void startRelease();
    void release();
    void stopRelease();

    void setKeyState(int symkey, bool pressed);
};


#endif //REMOTE_DESKTOP_VIRTUAL_KEYBOARD_H
