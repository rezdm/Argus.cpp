#pragma once

#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>
#include <type_traits>

class thread_pool {
public:
    explicit thread_pool(size_t num_threads = std::thread::hardware_concurrency());
    ~thread_pool();

    // Submit a task and get a future for the result
    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args)
        -> std::future<typename std::invoke_result_t<F, Args...>>;

    // Get current number of threads
    [[nodiscard]] size_t thread_count() const noexcept { return workers_.size(); }

    // Get number of pending tasks
    [[nodiscard]] size_t pending_tasks() const;

    // Check if pool is shutting down
    [[nodiscard]] bool is_stopping() const noexcept { return stop_; }

private:
    // Worker threads
    std::vector<std::thread> workers_;

    // Task queue
    std::queue<std::function<void()>> tasks_;

    // Synchronization
    mutable std::mutex queue_mutex_;
    std::condition_variable condition_;
    std::atomic<bool> stop_;
};

// Template implementation must be in header
template<class F, class... Args>
auto thread_pool::enqueue(F&& f, Args&&... args)
    -> std::future<typename std::invoke_result_t<F, Args...>> {

    using return_type = typename std::invoke_result_t<F, Args...>;

    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );

    std::future<return_type> result = task->get_future();

    {
        std::unique_lock<std::mutex> lock(queue_mutex_);

        // Don't allow enqueueing after stopping the pool
        if (stop_) {
            throw std::runtime_error("enqueue on stopped ThreadPool");
        }

        tasks_.emplace([task](){ (*task)(); });
    }

    condition_.notify_one();
    return result;
}