// tcp_client_receiver.h
#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include "cJSON.h"
#include "message_queue.h"

class tcp_client_receiver {
public:
    tcp_client_receiver(message_queue& mq, const std::string& server_ip, int server_port);
    ~tcp_client_receiver();

    void start();
    void stop();

    // 新增发送 JSON 消息的方法
    bool send_json(const cJSON* jsonRoot, bool isLora = false);

private:
    message_queue& mq_;
    std::string server_ip_;
    int server_port_;
    std::atomic<bool> is_running_;
    int client_fd_;
    std::thread receive_thread_;
    std::mutex send_mutex_;  // 保证发送线程安全

    bool do_connect();
    void receive_loop();
    void connect_and_receive();

    // 新增帧构建函数
    unsigned char* buildFrame(
        unsigned char frameHeader,
        const void* content,
        int contentLength,
        int& frameLength
    );
};