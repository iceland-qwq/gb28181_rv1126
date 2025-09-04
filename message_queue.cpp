//
// Created by liketao on 25-8-26.
//

#include "message_queue.h"

#include <iostream>

void message_queue::push(const Message& msg) {
    std::lock_guard<std::mutex> lock(mtx_);
    queue_.push(msg);
    cv_.notify_one();  // 唤醒一个等待的消费者线程
}

Message message_queue::pop() {
    std::unique_lock<std::mutex> lock(mtx_);
    
    // 等待条件：队列非空 或 停止标志被设置
    cv_.wait(lock, [this] { return !queue_.empty() || stop_; });

    // 如果已停止且队列为空，返回退出标记
    if (stop_ && queue_.empty()) {
        return {-1, {},0};
    }

    Message msg = queue_.front();
    queue_.pop();
    return msg;
}

void message_queue::stop() {
    stop_ = true;
    cv_.notify_all();  // 通知所有等待线程（如多个消费者）
}