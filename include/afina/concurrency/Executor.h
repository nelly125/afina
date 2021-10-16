#ifndef AFINA_CONCURRENCY_EXECUTOR_H
#define AFINA_CONCURRENCY_EXECUTOR_H

#include <algorithm>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>

namespace Afina {
namespace Concurrency {

/**
 * # Thread pool
 */
class Executor {
    enum class State {
        // Threadpool is fully operational, tasks could be added and get executed
        kRun,

        // Threadpool is on the way to be shutdown, no ned task could be added, but existing will be
        // completed as requested
        kStopping,

        // Threadppol is stopped
        kStopped
    };

    Executor(std::string name, std::size_t low_watermark, std::size_t hight_watermark, std::size_t max_queue_size,
             std::size_t idle_time)
        : _name(std::move(name)), _low_watermark(low_watermark), _hight_watermark(hight_watermark),
          _max_queue_size(max_queue_size), _idle_time(idle_time), _working_threads(0) {
        std::unique_lock<std::mutex> _lock(mutex);
        for (std::size_t i = 0; i < _low_watermark; i++) {
            threads.emplace_back(std::thread([this] { return perform(this); }));
        }
        state = State::kRun;
    }
    ~Executor() {
        /*        if (state != State::kRun) {
                    Stop(true);
                } else {
                    throw std::runtime_error("Executor is already stopped");
                }*/
        Stop(true);
    }

    /**
     * Signal thread pool to stop, it will stop accepting new jobs and close threads just after each become
     * free. All enqueued jobs will be complete.
     *
     * In case if await flag is true, call won't return until all background jobs are done and all threads are stopped
     */
    void Stop(bool await = false) {
        if (state == State::kStopped) {
            return;
        }

        {
            std::unique_lock<std::mutex> lock(mutex);
            state = State::kStopping;
            empty_condition.notify_all();
            if (await && !threads.empty()) {
                while (!threads.empty()) {
                    _stop_condition.wait(lock);
                }
            }
            if (_working_threads == 0) {
                state == State::kStopped;
            }
        }
    }

    /**
     * Add function to be executed on the threadpool. Method returns true in case if task has been placed
     * onto execution queue, i.e scheduled for execution and false otherwise.
     *
     * That function doesn't wait for function result. Function could always be written in a way to notify caller about
     * execution finished by itself
     */
    template <typename F, typename... Types> bool Execute(F &&func, Types... args) {
        // Prepare "task"
        auto exec = std::bind(std::forward<F>(func), std::forward<Types>(args)...);

        std::unique_lock<std::mutex> lock(this->mutex);
        if (state != State::kRun || threads.size() == _hight_watermark || tasks.size() == _max_queue_size) {
            return false;
        }

        // Enqueue new task

        tasks.push_back(exec);
        if ((threads.size() < _hight_watermark) && (threads.size() == _working_threads)) {
            threads.emplace_back(std::thread([this] { return perform(this); }));
        }
        empty_condition.notify_one();
        return true;
    }

private:
    // No copy/move/assign allowed
    Executor(const Executor &);            // = delete;
    Executor(Executor &&);                 // = delete;
    Executor &operator=(const Executor &); // = delete;
    Executor &operator=(Executor &&);      // = delete;

    /**
     * Main function that all pool threads are running. It polls internal task queue and execute tasks
     */
    friend void perform(Executor *executor) {
        std::unique_lock<std::mutex> lock(executor->mutex);
        bool flag = false;
        while ((executor->state == State::kRun) || (!executor->tasks.empty())) {
            auto now = std::chrono::system_clock::now();
            std::function<void()> task;
            while (executor->tasks.empty()) {
                auto cv_time =
                    executor->empty_condition.wait_until(lock, now + std::chrono::milliseconds(executor->_idle_time));
                if ((cv_time == std::cv_status::timeout && executor->tasks.empty() &&
                     executor->threads.size() > executor->_low_watermark) ||
                    executor->state != State::kRun) {
                    flag = true;
                    break;
                }
            }
            if (flag && executor->tasks.empty()) {
                break;
            }
            task = std::move(executor->tasks.front());
            executor->tasks.pop_front();
            executor->_working_threads++;
            lock.unlock();
            try {
                task();
            } catch (const std::exception &exc) {
                std::cout << exc.what() << std::endl;
            }
            lock.unlock();
            executor->_working_threads--;
        }

        auto thread_id = std::this_thread::get_id();
        auto it = std::find_if(executor->threads.begin(), executor->threads.end(),
                               [thread_id](std::thread &thr) { return thr.get_id() == thread_id; });
        (*it).detach();
        executor->threads.erase(it);
        if (executor->threads.empty() && executor->state != Executor::State::kRun) {
            executor->state = Executor::State::kStopped;
            executor->_stop_condition.notify_all();
        }
    }

    /**
     * Mutex to protect state below from concurrent modification
     */
    std::mutex mutex;

    /**
     * Conditional variable to await new data in case of empty queue
     */
    std::condition_variable empty_condition;

    /**
     * Vector of actual threads that perorm execution
     */
    std::vector<std::thread> threads;

    /**
     * Task queue
     */
    std::deque<std::function<void()>> tasks;

    /**
     * Flag to stop bg threads
     */
    State state;

    std::condition_variable _stop_condition;

    std::string _name;
    std::size_t _low_watermark;
    std::size_t _working_threads;
    std::size_t _count_threads;
    std::size_t _hight_watermark;
    std::size_t _max_queue_size;
    std::size_t _idle_time;
    //    std::unordered_map <std::thread::id, std::thread> _workers;
};

} // namespace Concurrency
} // namespace Afina

#endif // AFINA_CONCURRENCY_EXECUTOR_H
