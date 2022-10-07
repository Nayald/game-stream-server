extern "C" {
    #include <SDL2/SDL.h>
}

#include <unistd.h>
#include <fcntl.h>

#include <iostream>
#include <unordered_map>
#include <vector>

#include "virtual_gamepad.h"
#include "../exception.h"

const auto INPUT_RELEASE_DELAY = std::chrono::milliseconds(50);

/*const std::unordered_map<int, int> SDL2LINUX_GAMEPAD = {
        {SDL_CONTROLLER_BUTTON_A,             BTN_A},
        {SDL_CONTROLLER_BUTTON_B,             BTN_B},
        {SDL_CONTROLLER_BUTTON_X,             BTN_X},
        {SDL_CONTROLLER_BUTTON_Y,             BTN_Y},
        {SDL_CONTROLLER_BUTTON_BACK,          BTN_SELECT},
        {SDL_CONTROLLER_BUTTON_GUIDE,         BTN_MODE},
        {SDL_CONTROLLER_BUTTON_START,         BTN_START},
        {SDL_CONTROLLER_BUTTON_LEFTSTICK,     BTN_THUMBL},
        {SDL_CONTROLLER_BUTTON_RIGHTSTICK,    BTN_THUMBR},
        {SDL_CONTROLLER_BUTTON_LEFTSHOULDER,  BTN_TL},
        {SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, BTN_TR},
        {SDL_CONTROLLER_BUTTON_DPAD_UP,       BTN_DPAD_UP},
        {SDL_CONTROLLER_BUTTON_DPAD_DOWN,     BTN_DPAD_DOWN},
        {SDL_CONTROLLER_BUTTON_DPAD_LEFT,     BTN_DPAD_LEFT},
        {SDL_CONTROLLER_BUTTON_DPAD_RIGHT,    BTN_DPAD_RIGHT},
};*/

constexpr std::array<uint16_t, 15> SDL2LINUX_GAMEPAD = {
        BTN_A,
        BTN_B,
        BTN_X,
        BTN_Y,
        BTN_SELECT,
        BTN_MODE,
        BTN_START,
        BTN_THUMBL,
        BTN_THUMBR,
        BTN_TL,
        BTN_TR,
        BTN_DPAD_UP,
        BTN_DPAD_DOWN,
        BTN_DPAD_LEFT,
        BTN_DPAD_RIGHT,
};

std::atomic<size_t> VirtualGamepad::counter = {0};

VirtualGamepad::VirtualGamepad() : idx(counter++) {

}

VirtualGamepad::~VirtualGamepad() {
    ioctl(fd, UI_DEV_DESTROY);
    close(fd);
}

void VirtualGamepad::init() {
    fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (fd < 0) {
        throw InitFail("unable to open /dev/uinput");
    }

    if (ioctl(fd, UI_SET_EVBIT, EV_ABS) < 0) {
        throw InitFail("fail to set EV_REL bit");
    }

    uinput_abs_setup abs_setup[2] = {
            {ABS_X, {0, -32767, 32767, 0, 0, 0}},
            {ABS_Y, {0, -32767, 32767, 0, 0, 0}}
    };
    if (ioctl(fd, UI_SET_ABSBIT, ABS_X) < 0 || ioctl(fd, UI_ABS_SETUP, abs_setup) < 0 ||
        ioctl(fd, UI_SET_ABSBIT, ABS_Y) < 0 || ioctl(fd, UI_ABS_SETUP, abs_setup + 1) < 0) {
        throw InitFail("fail to set gamepad left stick axis");
    }

    abs_setup[0].code = ABS_RX;
    abs_setup[1].code = ABS_RY;
    if (ioctl(fd, UI_SET_ABSBIT, ABS_RX) < 0 || ioctl(fd, UI_ABS_SETUP, abs_setup) < 0 ||
        ioctl(fd, UI_SET_ABSBIT, ABS_RY) < 0 || ioctl(fd, UI_ABS_SETUP, abs_setup + 1) < 0) {
        throw InitFail("fail to set gamepad right stick axis");
    }

    abs_setup[0].code = ABS_Z;
    abs_setup[0].absinfo.minimum = 0;
    abs_setup[1].code = ABS_RZ;
    abs_setup[1].absinfo.minimum = 0;
    if (ioctl(fd, UI_SET_ABSBIT, ABS_Z) < 0 || ioctl(fd, UI_ABS_SETUP, abs_setup) < 0 ||
        ioctl(fd, UI_SET_ABSBIT, ABS_RZ) < 0 || ioctl(fd, UI_ABS_SETUP, abs_setup + 1) < 0) {
        throw InitFail("fail to set gamepad triggers axis");
    }

    if (ioctl(fd, UI_SET_EVBIT, EV_KEY) < 0) {
        throw InitFail("fail to set EV_KEY bit");
    }

    // gamepad buttons
    for (int val : SDL2LINUX_GAMEPAD) {
        if (ioctl(fd, UI_SET_KEYBIT, val) < 0) {
            throw InitFail("fail to set a button code");
        }
    }

    uinput_setup usetup = {{BUS_USB, 0x1234, 0x5678, 2}, "Remote Desktop Virtual Gamepad", 0};
    std::sprintf(usetup.name + 30, " %zu", idx);
    if (ioctl(fd, UI_DEV_SETUP, &usetup) < 0 || ioctl(fd, UI_DEV_CREATE) < 0) {
        throw InitFail("fail to create virtual gamepad");
    }

    std::cerr << "gamepad " << idx << ": initialized" << std::endl;
}

void VirtualGamepad::startRelease() {
    if (initialized && release_stop_condition.load(std::memory_order_relaxed)) {
        release_stop_condition.store(false, std::memory_order_relaxed);
        release_thread = std::thread(&VirtualGamepad::release, this);
    } else {
        std::cout << name << ": not initialized or thread already running" << std::endl;
    }
}

void VirtualGamepad::stopRelease() {
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

void VirtualGamepad::release() {
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
                events.push_back({{0, 0}, EV_KEY, (unsigned short) it->first, 0});
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

void VirtualGamepad::setPositionAbsolute(int x, int y, int rx, int ry, int z, int rz) {
    input_event e[] = {
            {{0, 0}, EV_ABS, ABS_X,      x},
            {{0, 0}, EV_ABS, ABS_Y,      y},
            {{0, 0}, EV_ABS, ABS_RX,     rx},
            {{0, 0}, EV_ABS, ABS_RY,     ry},
            {{0, 0}, EV_ABS, ABS_Z,      z},
            {{0, 0}, EV_ABS, ABS_RZ,     rz},
            {{0, 0}, EV_SYN, SYN_REPORT, 0},
    };
    lock.lock();
    write(fd, &e, sizeof(e));
    lock.unlock();
}

void VirtualGamepad::setButtonStates(int button_states) {
    std::vector<input_event> events;
    events.reserve(6);
    lock.lock();
    for (size_t i = 0; i < SDL2LINUX_GAMEPAD.size(); ++i) {
        bool pressed = button_states & (1 << i);
        auto exist = pressed_buttons.find(SDL2LINUX_GAMEPAD[i]) != pressed_buttons.end();
        if (pressed) {
            pressed_buttons.insert_or_assign(SDL2LINUX_GAMEPAD[i], std::chrono::steady_clock::now());
        } else {
            pressed_buttons.erase(SDL2LINUX_GAMEPAD[i]);
        }

        if (pressed ^ exist) {
            events.push_back({{0, 0}, EV_KEY, SDL2LINUX_GAMEPAD[i], pressed});
        }
    }

    if (!events.empty()) {
        events.push_back({{0, 0}, EV_SYN, SYN_REPORT, 0});
    }

    write(fd, events.data(), sizeof(input_event) * events.size());
    lock.unlock();
}