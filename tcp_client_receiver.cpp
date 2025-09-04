//
// Created by liketao on 25-9-2.
//


// tcp_client_receiver.cpp
#include "tcp_client_receiver.h"
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>

tcp_client_receiver::tcp_client_receiver(message_queue& mq, const std::string& server_ip, int server_port)
    : mq_(mq), server_ip_(server_ip), server_port_(server_port),
      is_running_(false), client_fd_(-1) {}

tcp_client_receiver::~tcp_client_receiver() {
    stop();
}

void tcp_client_receiver::start() {
    is_running_ = true;
    receive_thread_ = std::thread(&tcp_client_receiver::connect_and_receive, this);
}

void tcp_client_receiver::stop() {
    is_running_ = false;
    if (client_fd_ >= 0) {
        close(client_fd_);
    }
    if (receive_thread_.joinable()) {
        receive_thread_.join();
    }
}

void tcp_client_receiver::connect_and_receive() {
    while (is_running_) {
        // 创建 socket
        client_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (client_fd_ < 0) {
            std::cerr << "Socket creation failed\n";
            return;
        }

        struct sockaddr_in serv_addr{};
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(server_port_);
        if (inet_pton(AF_INET, server_ip_.c_str(), &serv_addr.sin_addr) <= 0) {
            std::cerr << "Invalid address: " << server_ip_ << std::endl;
            close(client_fd_);
            return;
        }

        if (connect(client_fd_, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
            std::cerr << "Connect failed, retrying...\n";
            close(client_fd_);
            sleep(2);  // 避免重连过快
            continue;
        }

        std::cout << "Connected to server " << server_ip_ << ":" << server_port_ << "\n";

        uint8_t buffer[512];
        while (is_running_) {
            ssize_t bytes_received = recv(client_fd_, buffer, sizeof(buffer), 0);
            if (bytes_received > 0) {
                Message msg;
                msg.type = 0;
                msg.size = bytes_received;
                memcpy(msg.data, buffer, bytes_received);
                mq_.push(msg);
            } else if (bytes_received == 0) {
                std::cout << "Server disconnected. Reconnecting...\n";
                break;  // 跳出内层循环，重新 connect
            } else {
                std::cerr << "Receive error: " << strerror(errno) << std::endl;
                break;
            }
        }

        close(client_fd_);
        client_fd_ = -1;

        if (!is_running_) break;
        sleep(1);
    }
}