#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <iostream>

#include "socket_server.h"
#include "../exception.h"
#include "../video/H264Encoder.h"

constexpr auto LOOKUP_DELAY = std::chrono::seconds(1);
constexpr auto NOTIFY_DEADLINE_DELAY = std::chrono::seconds(15);

SocketServer::SocketServer(Encoder &audio_enc, Encoder &video_enc) : name("socket server"), audio_enc(audio_enc), video_enc(video_enc) {

}

SocketServer::~SocketServer() {
    std::cout << name << ": next lines are triggered by ~SocketServer() call" << std::endl;
    stop();
    if (sockfd > 0) {
        close(sockfd);
        sockfd = -1;
    }

    auto it = sessions.begin();
    while (it != sessions.end()) {
        std::cout << "purge session for " << (it->first & 0xFF) << '.' << (it->first >> 8 & 0xFF) << '.'
                  << (it->first >> 16 & 0xFF) << '.' << (it->first >> 24 & 0xFF) << std::endl;
        it->second.stop();
        audio_enc.detachSink(&it->second.getRtpAudio());
        video_enc.detachSink(&it->second.getRtpVideo());
        it = sessions.erase(it);
    }
}

void SocketServer::init() {
    if (sockfd > 0) {
        close(sockfd);
        sockfd = -1;
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        throw InitFail("socket creation failed");
    }

    sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(9999);

    const int enable = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT | SO_REUSEADDR | SO_DEBUG, &enable, sizeof(enable)) < 0) {
        throw InitFail("fail to reuse tcp port");
    }

    if (bind(sockfd, (const struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        throw InitFail("fail to bind tcp port");
    }

    if (listen(sockfd, 5) < 0) {
        throw InitFail("fail to listen on tcp socket");
    }

    initialized = true;
}

void SocketServer::start() {
    if (initialized && stop_condition.load(std::memory_order_relaxed)) {
        stop_condition.store(false, std::memory_order_relaxed);
        listen_thread = std::thread(&SocketServer::listenSocket, this);
        purge_thread = std::thread(&SocketServer::purge, this);
    } else {
        std::cout << name << ": not initialized or threads already running" << std::endl;
    }
}

void SocketServer::stop() {
    if (!stop_condition.load(std::memory_order_relaxed)) {
        stop_condition.store(true, std::memory_order_relaxed);
        if (listen_thread.joinable()) {
            listen_thread.join();
        } else {
            std::cout << name << ": listen thread is not joinable" << std::endl;
        }

        if (purge_thread.joinable()) {
            purge_thread.join();
        } else {
            std::cout << name << ": purge thread is not joinable" << std::endl;
        }
    } else {
        std::cout << name << ": threads are not running" << std::endl;
    }
}

void SocketServer::listenSocket() {
    std::cerr << name << ": listen thread pid is " << gettid() << std::endl;
    fd_set fds;
    timeval tv;
    int client_socket;
    sockaddr_in client_address;
    socklen_t client_address_size;
    while (!stop_condition.load(std::memory_order_relaxed)) {
        FD_ZERO(&fds);
        FD_SET(sockfd, &fds);
        FD_SET(sockfd, &fds);
        tv.tv_sec = 0;
        tv.tv_usec = 100'000;
        int res = select(sockfd + 1, &fds, NULL, NULL, &tv);
        if (res < 0) {
            throw RunError("error while waiting for connection");
        } else if (res == 0) {
            continue;
        } else {
            client_address_size = sizeof(client_address);
            client_socket = accept(sockfd, (sockaddr*)&client_address, &client_address_size);
            if (client_socket < 0) {
                throw RunError("fail to accept tcp socket");
            }

            lock.lock();
            auto it = sessions.find(client_address.sin_addr.s_addr);
            if (it != sessions.end()) {
                std::cout << "new sessions from existing client (" << (it->first & 0xFF) << '.'
                << (it->first >> 8 & 0xFF) << '.' << (it->first >> 16 & 0xFF) << '.'
                << (it->first >> 24 & 0xFF) << "), renew" << std::endl;
                it->second.stop();
                audio_enc.detachSink(&it->second.getRtpAudio());
                video_enc.detachSink(&it->second.getRtpVideo());
                sessions.erase(it);
            }

            auto res = sessions.emplace(std::piecewise_construct,
                                        std::forward_as_tuple(client_address.sin_addr.s_addr),
                                        std::forward_as_tuple(client_address, client_socket));
            lock.unlock();
            if (!res.second) {
                continue;
            }

            res.first->second.init(audio_enc.getContext(), video_enc.getContext());
            res.first->second.start();
            audio_enc.attachSink(&res.first->second.getRtpAudio());
            video_enc.attachSink(&res.first->second.getRtpVideo());
            res.first->second.attachSink(reinterpret_cast<H264Encoder*>(&video_enc));
        }
    }
}

void SocketServer::purge() {
    std::cerr << name << ": purge thread pid is " << gettid() << std::endl;
    while (!stop_condition.load(std::memory_order_relaxed)) {
        std::this_thread::sleep_for(LOOKUP_DELAY);
        const auto current_time = std::chrono::steady_clock::now();
        lock.lock();
        auto it = sessions.begin();
        while (it != sessions.end()) {
            if ((NOTIFY_DEADLINE_DELAY + it->second.getLastUpdate() - current_time).count() < 0) {
                std::cout << "purge session for " << (it->first & 0xFF) << '.' << (it->first >> 8 & 0xFF) << '.'
                          << (it->first >> 16 & 0xFF) << '.' << (it->first >> 24 & 0xFF) << std::endl;
                it->second.stop();
                audio_enc.detachSink(&it->second.getRtpAudio());
                video_enc.detachSink(&it->second.getRtpVideo());
                it = sessions.erase(it);
            } else {
                ++it;
            }
        }
        lock.unlock();
    }
}
