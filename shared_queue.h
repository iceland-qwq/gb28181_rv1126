//
// Created by liketao on 25-6-29.
//
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>

const size_t BUFFER_SIZE = 1024 * 1024; // 1MB缓冲区
using Buffer = std::unique_ptr<char[]>; // 使用智能指针管理内存

typedef struct{
    Buffer buf;
    int size ;
    bool isIFrame;
}video_packet;

class SharedQueue {
public:
    // 入队操作
    void enqueue(video_packet&& packet);

    // 出队操作
    video_packet dequeue();
    void clear();

private:
    std::queue<video_packet> queue_;
    std::mutex mutex_;
    std::condition_variable cond_var_;
};
