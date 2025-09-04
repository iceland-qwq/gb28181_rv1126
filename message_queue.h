
#pragma once

#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>

// 1. 定义消息结构
struct Message {
    int type;
    uint8_t  data[512];
    int size ;
};

// 2. 线程安全的消息队列类声明
class message_queue {
private:
    std::queue<Message> queue_;
    std::mutex mtx_;
    std::condition_variable cv_;
    std::atomic<bool> stop_{false};

public:
    // 发送消息（入队）
    void push(const Message& msg);

    // 接收消息（出队，阻塞等待）
    Message pop();

    // 停止队列，唤醒所有等待线程
    void stop();
};