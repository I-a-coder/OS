// my_logger.cpp
// Custom Logger Library Implementation

#include "my_logger.h"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>

// Global logger instance
std::shared_ptr<MyLogger> g_logger = nullptr;

// Constructor
MyLogger::MyLogger(const std::string& file) 
    : filename(file), is_open(false) {
    open(file);
}

// Destructor
MyLogger::~MyLogger() {
    close();
}

// Open log file
bool MyLogger::open(const std::string& file) {
    std::lock_guard<std::mutex> lock(mtx);
    
    if (is_open) {
        log_file.close();
    }
    
    filename = file;
    log_file.open(filename, std::ios::out | std::ios::app);
    
    if (log_file.is_open()) {
        is_open = true;
        return true;
    }
    
    is_open = false;
    return false;
}

// Close log file
void MyLogger::close() {
    std::lock_guard<std::mutex> lock(mtx);
    
    if (is_open && log_file.is_open()) {
        log_file.flush();
        log_file.close();
        is_open = false;
    }
}

// Get current timestamp
std::string MyLogger::get_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    
    return ss.str();
}

// Write log entry
void MyLogger::write_log(const std::string& level, const std::string& message) {
    std::lock_guard<std::mutex> lock(mtx);
    
    if (!is_open || !log_file.is_open()) {
        std::cerr << "Logger is not open!" << std::endl;
        return;
    }
    
    // Get timestamp
    std::string timestamp = get_timestamp();
    
    // Write to file with flush (causes performance bottleneck)
    log_file << "[" << timestamp << "] [" << level << "] " << message << std::endl;
    
    // std::endl causes immediate flush - this is the bottleneck!
}

// Info log
void MyLogger::info(const std::string& message) {
    write_log("INFO", message);
}

// Warning log
void MyLogger::warning(const std::string& message) {
    write_log("WARNING", message);
}

// Error log
void MyLogger::error(const std::string& message) {
    write_log("ERROR", message);
}

// Debug log
void MyLogger::debug(const std::string& message) {
    write_log("DEBUG", message);
}

// Flush logs
void MyLogger::flush() {
    std::lock_guard<std::mutex> lock(mtx);
    if (is_open && log_file.is_open()) {
        log_file.flush();
    }
}

// Initialize global logger
void init_logger(const std::string& filename) {
    g_logger = std::make_shared<MyLogger>(filename);
}

// Helper functions
void log_info(const std::string& message) {
    if (g_logger) {
        g_logger->info(message);
    }
}

void log_warning(const std::string& message) {
    if (g_logger) {
        g_logger->warning(message);
    }
}

void log_error(const std::string& message) {
    if (g_logger) {
        g_logger->error(message);
    }
}

void log_debug(const std::string& message) {
    if (g_logger) {
        g_logger->debug(message);
    }
}
