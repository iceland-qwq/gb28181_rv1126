#include "shared_queue.h"



void SharedQueue::enqueue(video_packet&& packet) {
    std::lock_guard<std::mutex> lock(mutex_);

    queue_.push(std::move(packet));
    //printf("data enqueue\n");
    cond_var_.notify_one(); // 通知等待的消费者
}

video_packet SharedQueue::dequeue() {
    std::unique_lock<std::mutex> lock(mutex_);
    cond_var_.wait(lock, [this] { return !queue_.empty(); }); // 等待队列非空

    video_packet packet = std::move(queue_.front());
    queue_.pop();
    //printf("data dequeue\n");
    return packet;
}

void SharedQueue::clear() {
    std::lock_guard<std::mutex> lock(mutex_);

    // 简单地清空队列
    while (!queue_.empty()) {
        queue_.pop();
    }
    // 或者更简洁：
    // std::queue<video_packet>{}.swap(queue_);

    printf("SharedQueue cleared.\n");
}
