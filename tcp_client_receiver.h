// tcp_client_receiver.h
#pragma once
#include "message_queue.h"
#include <thread>
#include <atomic>
#include <string>

class tcp_client_receiver {
public:
    tcp_client_receiver(message_queue& mq, const std::string& server_ip, int server_port);
    ~tcp_client_receiver();

    void start();
    void stop();

private:
    void connect_and_receive();

    message_queue& mq_;
    std::string server_ip_;
    int server_port_;

    std::atomic<bool> is_running_;
    std::thread receive_thread_;
    int client_fd_;
};