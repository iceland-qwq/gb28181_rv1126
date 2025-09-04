
#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <vector>
#include <functional>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>
#include "message_queue.h"

class tcp_receiver {
public:
    tcp_receiver(message_queue& mq, int port = 8080);
    ~tcp_receiver();

    void start();  // 启动 TCP 服务器
    void stop();   // 停止服务器

private:
    void accept_connections();  // 接受客户端连接
    void handle_client(int client_fd);  // 处理单个客户端连接

    message_queue& mq_;  // 消息队列引用
    int server_fd_;
    std::atomic<bool> is_running_;
    std::thread server_thread_;
    std::vector<std::thread> client_threads_;
};