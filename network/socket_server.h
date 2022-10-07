//
// Created by madynes on 24/08/2022.
//

#ifndef REMOTE_DESKTOP_SOCKET_SERVER_H
#define REMOTE_DESKTOP_SOCKET_SERVER_H
#include <unordered_map>
#include <thread>
#include <atomic>

#include "../Encoder.h"
#include "remote_session.h"
#include "../spinlock.h"

class SocketServer {
    std::string name;
    bool initialized = false;

    Encoder &audio_enc;
    Encoder &video_enc;

    int sockfd = -1;

    std::unordered_map<uint32_t, RemoteSession> sessions;
    spinlock lock;

    std::atomic<bool> stop_condition = true;
    std::thread listen_thread;
    std::thread purge_thread;

public:
    SocketServer(Encoder &audio_enc, Encoder &video_enc);
    ~SocketServer();

    void init();

    void start();
    void stop();

    void listenSocket();
    void purge();
};


#endif //REMOTE_DESKTOP_SOCKET_SERVER_H
