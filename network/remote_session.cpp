#include <arpa/inet.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <csignal>

#include "remote_session.h"
#include "../exception.h"

constexpr size_t BUFFER_SIZE = 4096;

void appendJSONFormattedString(std::ostream &os, const std::string &s) {
    for (const char c : s) {
        switch (c) {
            case '\b':
                os << "\\b";
                break;
            case '\f':
                os << "\\f";
                break;
            case '\n':
                os << "\\n";
                break;
            case '\r':
                os << "\\r";
                break;
            case '\t':
                os << "\\t";
                break;
            case '"':
                os << "\\\"";
                break;
            case '\\':
                os << "\\\\";
                break;
            default:
                if ('\x00' <= c && c <= '\x1f') {
                    std::ios state(nullptr);
                    state.copyfmt(os);
                    os << "\\u" << std::hex << std::setw(4) << std::setfill('0') << c;
                    os.copyfmt(state);
                } else {
                    os << c;
                }
                break;
        }
    }
}

RemoteSession::RemoteSession(sockaddr_in remote_address, int tcp_socket) : remote_address(remote_address), tcp_socket(tcp_socket) {
    remote_address.sin_port = 9999;
}

RemoteSession::~RemoteSession() {
    close(tcp_socket);
}

void RemoteSession::init(AVCodecContext *audio_context, AVCodecContext *video_context) {
    udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_socket < 0) {
        throw InitFail("unable to create udp socket");
    }

    sockaddr_in server_address = {};

    // Filling server information
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(9999);

    const int enable = 1;
    if (setsockopt(udp_socket, SOL_SOCKET, SO_REUSEPORT | SO_REUSEADDR, &enable, sizeof(enable)) < 0) {
        throw InitFail("fail to reuse udp address/port");
    }

    /*timeval tv = {0, 100'000};
    if (setsockopt(udp_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)); < 0) {
        throw InitFail("fail to set timeout for udp socket");
    };*/

    if (bind(udp_socket, (const struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
        throw InitFail("fail to bind udp port");
    }

    if (connect(udp_socket, (const struct sockaddr*)&remote_address, sizeof(remote_address)) < 0) {
        throw InitFail("fail to set default address");
    }

    char buffer[32];
    std::stringstream ss;
    ss << R"({"t":"R","g":0,"r":")";
    std::sprintf(buffer, "rtp://%s:%d", inet_ntoa(remote_address.sin_addr), 10000);
    appendJSONFormattedString(ss, rtp_audio.init(buffer, audio_context));
    std::sprintf(buffer, "rtp://%s:%d", inet_ntoa(remote_address.sin_addr), 10002);
    appendJSONFormattedString(ss, rtp_video.init(buffer, video_context));

    keyboard.init();
    mouse.init();
    gamepad.init();

    initialized = true;
    std::cout << "new session for " << inet_ntoa(remote_address.sin_addr) << ": initialized" <<std::endl;
}

RTPAudioSender& RemoteSession::getRtpAudio() {
    return rtp_audio;
}

RTPVideoSender& RemoteSession::getRtpVideo() {
    return rtp_video;

}

void RemoteSession::start() {
    stop_condition = false;
    thread = std::thread(&RemoteSession::run, this);
    rtp_audio.start();
    rtp_video.start();
    keyboard.startRelease();
    mouse.startRelease();
}

void RemoteSession::run() {
    fd_set fds;
    timeval tv;
    alignas(64) char buffer[BUFFER_SIZE];
    alignas(64) char stream_buffer[2 * BUFFER_SIZE];
    size_t stream_size = 0;
    try {
        while (!stop_condition) {
            FD_ZERO(&fds);
            FD_SET(tcp_socket, &fds);
            FD_SET(udp_socket, &fds);
            tv.tv_sec = 0;
            tv.tv_usec = 100'000;
            int res = select(std::max(tcp_socket, udp_socket) + 1, &fds, NULL, NULL, &tv);
            if (res < 0) {
                std::cerr << errno << std::endl;
                throw RunError("error while waiting for data");
            } else if (res == 0) {
                continue;
            } else {
                if (FD_ISSET(tcp_socket, &fds)) {
                    if (stream_size >= sizeof(stream_buffer) - simdjson::SIMDJSON_PADDING) {
                        std::memmove(stream_buffer, stream_buffer + 1, stream_size -= 1);
                    }

                    ssize_t size = recv(tcp_socket, stream_buffer + stream_size, sizeof(stream_buffer) - stream_size, 0);
                    if (size > 0) {
                        stream_size += size;
                        size_t parsed_size = handleCommands(stream_buffer, stream_size, sizeof(stream_buffer));
                        if (parsed_size > 0) {
                            std::memmove(stream_buffer, stream_buffer + parsed_size, stream_size -= parsed_size);
                        }

                        last_update = std::chrono::steady_clock::now();
                    }
                }

                if (FD_ISSET(udp_socket, &fds)) {
                    ssize_t size = recv(udp_socket, buffer, sizeof(buffer), 0);
                    if (size > 0) {
                        handleInputs(buffer, size, sizeof(buffer));
                        last_update = std::chrono::steady_clock::now();
                    }
                }
            }
        }
    } catch (std::exception &e) {
        std::cerr << e.what() << std::endl;
    }
}

void RemoteSession::stop() {
    stop_condition = true;
    thread.join();
    rtp_audio.stop();
    rtp_video.stop();
    keyboard.stopRelease();
    mouse.stopRelease();
}

std::chrono::steady_clock::time_point RemoteSession::getLastUpdate() const {
    return last_update;
}

void RemoteSession::write(const std::string &msg) {
    writeImpl(msg.c_str(), msg.size());
}

void RemoteSession::write(const char *msg, size_t size) {
    writeImpl(msg, size);
}

size_t RemoteSession::handleCommands(char *buffer, size_t size, size_t capacity) {
    size_t parsed_bytes = 0;
    try {
        simdjson::ondemand::document document = parser.iterate(buffer, size, capacity);
        parsed_bytes = document.raw_json().value().size();
        const std::string_view type = document["t"];
        document.rewind();
        if (type == "k") {
            std::cout << "tcp keepalive from " << inet_ntoa(remote_address.sin_addr) << std::endl;
        } else if (type == "r") {
            const std::string_view query = document["q"];
            // client ask a rtp endpoint
            if (query == "rtp") {
                std::stringstream ss;
                ss << R"({"t":"R","g":1,"r":")";
                appendJSONFormattedString(ss, rtp_audio.generateSdp());
                ss << R"("})";
                write(ss.str());

                ss.str(std::string());
                ss.clear();
                ss << R"({"t":"R","g":0,"r":")";
                appendJSONFormattedString(ss, rtp_video.generateSdp());
                ss << R"("})";
                write(ss.str());
            }
        }
    } catch (const simdjson::simdjson_error &err) {
        std::cout << err.what() << std::endl;
    }

    return parsed_bytes;
}

void RemoteSession::handleInputs(char *buffer, size_t size, size_t capacity) {
    try {
        simdjson::ondemand::document document = parser.iterate(buffer, size, capacity);
        const std::string_view type = document["t"];
        document.rewind();
        if (type == "i") {
            for (auto item: document.get_object()) {
                simdjson::ondemand::raw_json_string key = item.key();
                if (key == "k") {
                    std::vector<std::vector<int>> keys;
                    for (auto arr: item.value()) {
                        keys.emplace_back();
                        for (int64_t e: arr) {
                            keys[keys.size() - 1].push_back(e);
                        }
                    }
                    for (auto e: keys[1]) {
                        keyboard.setKeyState(e, true);
                    }
                    for (auto e: keys[0]) {
                        keyboard.setKeyState(e, false);
                    }
                }

                if (key == "m") {
                    std::vector<int> pos;
                    for (int64_t e: item.value()) {
                        pos.push_back(e);
                    }
                    mouse.setPositionRelative(pos[0], pos[1]);
                }

                if (key == "b") {
                    int64_t mouse_buttons = item.value();
                    mouse.setButtonStates(mouse_buttons);
                }

                if (key == "w") {
                    std::vector<int> pos;
                    for (int64_t e: item.value()) {
                        pos.push_back(e);
                    }

                    mouse.setWheelPositionRelative(pos[0], pos[1]);
                }

                if (key == "a") {
                    std::vector<int> axis;
                    for (int64_t e: item.value()) {
                        axis.push_back(e);
                    }

                    gamepad.setPositionAbsolute(axis[0], axis[1], axis[2], axis[3], axis[4], axis[5]);
                }

                if (key == "c") {
                    int64_t gamepad_buttons = item.value();
                    gamepad.setButtonStates(gamepad_buttons);
                }
            }
        }
    } catch (const simdjson::simdjson_error &err) {
        std::cout << err.what() << std::endl;
        std::cout.write(buffer, size) << std::endl;
    }
}

void RemoteSession::writeImpl(const char *msg, size_t size) {
    udp_socket_lock.lock();
    if (send(tcp_socket, msg, size, 0) != size) {
        std::cout << "not all bytes was sent to " << inet_ntoa(remote_address.sin_addr) << ":" << remote_address.sin_port << std::endl;
    }
    udp_socket_lock.unlock();
}
