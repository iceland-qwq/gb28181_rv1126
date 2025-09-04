//
// Created by liketao on 25-8-27.
//

#include "tcp_receiver.h"
#include <iostream>

tcp_receiver::tcp_receiver(message_queue& mq, int port)
    : mq_(mq), server_fd_(-1), is_running_(false) {
    struct sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    // 创建 TCP socket
    if ((server_fd_ = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        throw std::runtime_error("Socket creation failed");
    }

    // 设置 SO_REUSEADDR 避免 Address already in use 错误
    int opt = 1;
    if (setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        close(server_fd_);
        throw std::runtime_error("setsockopt failed");
    }

    // 绑定地址
    if (bind(server_fd_, (struct sockaddr*)&address, sizeof(address)) < 0) {
        close(server_fd_);
        throw std::runtime_error("Bind failed");
    }

    // 开始监听
    if (listen(server_fd_, 5) < 0) {
        close(server_fd_);
        throw std::runtime_error("Listen failed");
    }
}

tcp_receiver::~tcp_receiver() {
    stop();
}

void tcp_receiver::start() {
    is_running_ = true;
    server_thread_ = std::thread(&tcp_receiver::accept_connections, this);
}

void tcp_receiver::stop() {
    is_running_ = false;

    if (server_fd_ >= 0) {
        close(server_fd_);
    }

    // 等待所有客户端线程结束
    for (auto& t : client_threads_) {
        if (t.joinable()) {
            t.join();
        }
    }

    if (server_thread_.joinable()) {
        server_thread_.join();
    }
}

void tcp_receiver::accept_connections() {
    while (is_running_) {
        struct sockaddr_in client_addr{};
        socklen_t client_addr_len = sizeof(client_addr);
        int client_fd = accept(server_fd_, (struct sockaddr*)&client_addr, &client_addr_len);

        if (client_fd < 0) {
            if (!is_running_) break;
            std::cerr << "Accept failed: " << strerror(errno) << std::endl;
            continue;
        }

        std::cout << "New connection from " << inet_ntoa(client_addr.sin_addr) << ":" << ntohs(client_addr.sin_port) << std::endl;

        // 启动新线程处理客户端
        client_threads_.emplace_back([this, client_fd]() {
            handle_client(client_fd);
        });
    }
}

void tcp_receiver::handle_client(int client_fd) {
    uint8_t buffer[512];
    while (is_running_) {
        std::memset(buffer, 0, sizeof(buffer));
        ssize_t bytes_received = recv(client_fd, buffer, sizeof(buffer), 0);

        if (bytes_received <= 0) {
            if (bytes_received == 0) {
                std::cout << "Client disconnected" << std::endl;
            } else {
                std::cerr << "Receive error: " << strerror(errno) << std::endl;
            }
            close(client_fd);
            return;
        }

        // 将数据封装为 Message 并推入队列
        Message msg;
        msg.type = 0;  // 类型可自定义（例如 0 表示 JSON 数据）
        msg.size = bytes_received;
        memcpy(msg.data ,buffer, bytes_received);

        mq_.push(msg);
    }
}