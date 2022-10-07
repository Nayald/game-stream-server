extern "C" {
    #include <SDL2/SDL.h>
}

#include <unistd.h>
#include <fcntl.h>

#include <iostream>
#include <unordered_map>
#include <vector>

#include "virtual_mouse.h"
#include "../exception.h"

constexpr auto INPUT_RELEASE_DELAY = std::chrono::milliseconds(50);
constexpr std::array<uint16_t, 5> SDL2LINUX_MOUSE = {BTN_LEFT, BTN_MIDDLE, BTN_RIGHT, BTN_SIDE, BTN_EXTRA};

std::atomic<size_t> VirtualMouse::counter = {0};

VirtualMouse::VirtualMouse() : idx(counter++) {

}

VirtualMouse::~VirtualMouse() {
    ioctl(fd, UI_DEV_DESTROY);
    close(fd);
}

void VirtualMouse::init() {
    fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (fd < 0) {
        throw InitFail("unable to open /dev/uinput");
    }

    // mouse axis
    if (ioctl(fd, UI_SET_EVBIT, EV_REL) < 0) {
        throw InitFail("fail to set EV_REL bit");
    }

    if (ioctl(fd, UI_SET_RELBIT, REL_X) < 0 || ioctl(fd, UI_SET_RELBIT, REL_Y) < 0) {
        throw InitFail("fail to set mouse axis");
    }

    if (ioctl(fd, UI_SET_RELBIT, REL_WHEEL) < 0 || ioctl(fd, UI_SET_RELBIT, REL_HWHEEL)) {
        throw InitFail("fail to set wheel axis");
    }

    if (ioctl(fd, UI_SET_EVBIT, EV_KEY) < 0) {
        throw InitFail("fail to set EV_KEY bit");
    }
    // mouse buttons
    for (int val : SDL2LINUX_MOUSE) {
        if (ioctl(fd, UI_SET_KEYBIT, val) < 0) {
            throw InitFail("fail to set a button code");
        }
    }

    uinput_setup usetup = {{BUS_USB, 0x1234, 0x5678, 1}, "Remote Desktop Virtual Mouse", 0};
    std::sprintf(usetup.name + 28, " %zu", idx);
    if (ioctl(fd, UI_DEV_SETUP, &usetup) < 0 || ioctl(fd, UI_DEV_CREATE) < 0) {
        throw InitFail("fail to create virtual mouse");
    }

    std::cerr << "mouse " << idx << ": initialized" << std::endl;
}

void VirtualMouse::startRelease() {
    if (initialized && release_stop_condition.load(std::memory_order_relaxed)) {
        release_stop_condition.store(false, std::memory_order_relaxed);
        release_thread = std::thread(&VirtualMouse::release, this);
    } else {
        std::cout << name << ": not initialized or thread already running" << std::endl;
    }
}

void VirtualMouse::stopRelease() {
    if (!release_stop_condition.load(std::memory_order_relaxed)) {
        release_stop_condition.store(true, std::memory_order_relaxed);
        if (release_thread.joinable()) {
            release_thread.join();
        } else {
            std::cout << name << ": thread is not joinable" << std::endl;
        }
    } else {
        std::cout << name << ": thread is not running" << std::endl;
    }
}

void VirtualMouse::release() {
    std::vector<input_event> events;
    std::chrono::steady_clock::time_point start;
    while (!release_stop_condition) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        start = std::chrono::steady_clock::now();
        lock.lock();
        size_t size = pressed_buttons.size();
        auto it = pressed_buttons.begin();
        while (it != pressed_buttons.end()) {
            if (it->second + INPUT_RELEASE_DELAY <= start) {
                events.push_back({{0, 0}, EV_KEY, (unsigned short)it->first, 0});
                it = pressed_buttons.erase(it);
            } else {
                ++it;
            }
        }

        if (size > pressed_buttons.size()) {
            std::cout << "mouse " << idx << ": released " << size - pressed_buttons.size() << " pressed button(s)" << std::endl;
        }

        events.push_back({{0, 0}, EV_SYN, SYN_REPORT, 0});
        write(fd, events.data(), sizeof(input_event) * events.size());
        lock.unlock();
    };
}

void VirtualMouse::setPositionRelative(int x, int y) {
    input_event e[] = {
            {{0, 0}, EV_REL, REL_X,      x},
            {{0, 0}, EV_REL, REL_Y,      y},
            {{0, 0}, EV_SYN, SYN_REPORT, 0},
    };
    lock.lock();
    write(fd, &e, sizeof(e));
    lock.unlock();
}

void VirtualMouse::setButtonStates(int button_states) {
    std::vector<input_event> events;
    events.reserve(6);
    lock.lock();
    for (size_t i = 0; i < SDL2LINUX_MOUSE.size(); ++i) {
        bool pressed = button_states & (1 << i);
        auto exist = pressed_buttons.find(SDL2LINUX_MOUSE[i]) != pressed_buttons.end();
        if (pressed) {
            pressed_buttons.insert_or_assign(SDL2LINUX_MOUSE[i], std::chrono::steady_clock::now());
        } else {
            pressed_buttons.erase(SDL2LINUX_MOUSE[i]);
        }

        if (pressed ^ exist) {
            events.push_back({{0, 0}, EV_KEY, SDL2LINUX_MOUSE[i], pressed});
        }
    }

    if (!events.empty()) {
        events.push_back({{0, 0}, EV_SYN, SYN_REPORT, 0});
    }

    write(fd, events.data(), sizeof(input_event) * events.size());
    lock.unlock();
}

void VirtualMouse::setWheelPositionRelative(int wx, int wy) {
    input_event e[] = {
            {{0, 0}, EV_REL, REL_HWHEEL, wx},
            {{0, 0}, EV_REL, REL_WHEEL,  wy},
            {{0, 0}, EV_SYN, SYN_REPORT, 0},
    };
    lock.lock();
    write(fd, &e, sizeof(e));
    lock.unlock();
}