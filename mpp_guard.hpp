#pragma once
#include <functional>
#include <vector>
#include <csignal>
#include <mutex>
#include <atomic>

class MppGuard {
public:
    using CleanupFn = std::function<void()>;

    // 单例：全局唯一清理器
    static MppGuard& instance() {
        static MppGuard g;
        return g;
    }

    // 注册清理动作
    void push(CleanupFn fn) {
        std::lock_guard<std::mutex> lock(mtx_);
        cleanups_.push_back(std::move(fn));
    }

    // 立即执行所有清理动作（线程安全）
    void cleanup() {
        std::lock_guard<std::mutex> lock(mtx_);
        if (cleaned_.exchange(true)) return; // 只执行一次
        for (auto it = cleanups_.rbegin(); it != cleanups_.rend(); ++it) {
            try { (*it)(); } catch (...) {}
        }
        cleanups_.clear();
    }

    // 注册信号处理
    void install_signal_handlers() {
        std::signal(SIGINT,  on_signal);
        std::signal(SIGTERM, on_signal);
        std::signal(SIGSEGV, on_signal);
    }

private:
    MppGuard() = default;
    ~MppGuard() { cleanup(); }

    static void on_signal(int) { instance().cleanup(); }

    std::vector<CleanupFn> cleanups_;
    std::mutex             mtx_;
    std::atomic_bool       cleaned_{false};
};