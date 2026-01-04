#ifndef FAST_LOGGER_H
#define FAST_LOGGER_H

#include <fstream>
#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>

class FastLogger {
private:
    std::ofstream log_file;
    std::queue<std::string> log_queue;
    std::mutex queue_mutex;
    std::condition_variable cv;
    std::atomic<bool> running;
    std::thread worker_thread;

    // Background worker thread that writes logs to file
    void process_logs() {
        while (running || !log_queue.empty()) {
            std::unique_lock<std::mutex> lock(queue_mutex);
            
            cv.wait_for(lock, std::chrono::milliseconds(10), [this] {
                return !log_queue.empty() || !running;
            });
            
            // Process all available logs in batch
            while (!log_queue.empty()) {
                std::string entry = log_queue.front();
                log_queue.pop();
                
                // Unlock while writing to file (I/O operation)
                lock.unlock();
                
                if (log_file.is_open()) {
                    log_file << entry << std::endl;
                }
                
                lock.lock();
            }
        }
    }

public:
    FastLogger(const std::string& filename) : running(true) {
        log_file.open(filename, std::ios::app);
        worker_thread = std::thread(&FastLogger::process_logs, this);
    }

    ~FastLogger() {
        running = false;
        cv.notify_one();
        if (worker_thread.joinable()) {
            worker_thread.join();
        }
        if (log_file.is_open()) {
            log_file.close();
        }
    }

    void log(const std::string& message) {
        std::lock_guard<std::mutex> lock(queue_mutex);
        log_queue.push(message);
        cv.notify_one();
    }
};

#endif
