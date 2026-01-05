// my_logger.cpp
// Custom Logger Library Implementation

#include "my_logger.h"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>

// Define the global logger instance
std::shared_ptr<MyLogger> g_logger = nullptr;

// Constructor
MyLogger::MyLogger(const std::string& file) : filename(file), is_open(false) {
    open(file);
}

// Destructor
MyLogger::~MyLogger() {
    close();
}

// Open log file
bool MyLogger::open(const std::string& file) {
    std::lock_guard<std::mutex> lock(mtx);
    
    // Close existing file if open
    if (log_file.is_open()) {
        log_file.close();
    }
    
    filename = file;
    log_file.open(filename, std::ios::app);  // Open in append mode
    
    if (log_file.is_open()) {
        is_open = true;
        log_file << "\n========== Logger Session Started: " 
                 << get_timestamp() << " ==========\n";
        return true;
    }
    
    is_open = false;
    std::cerr << "Error: Failed to open log file: " << filename << std::endl;
    return false;
}

// Close log file
void MyLogger::close() {
    std::lock_guard<std::mutex> lock(mtx);
    
    if (log_file.is_open()) {
        log_file << "========== Logger Session Ended: " 
                 << get_timestamp() << " ==========\n\n";
        log_file.close();
        is_open = false;
    }
}

// Info level logging
void MyLogger::info(const std::string& message) {
    write_log("INFO", message);
}

// Warning level logging
void MyLogger::warning(const std::string& message) {
    write_log("WARNING", message);
}

// Error level logging
void MyLogger::error(const std::string& message) {
    write_log("ERROR", message);
}

// Debug level logging
void MyLogger::debug(const std::string& message) {
    write_log("DEBUG", message);
}

// Flush logs to disk
void MyLogger::flush() {
    std::lock_guard<std::mutex> lock(mtx);
    if (log_file.is_open()) {
        log_file.flush();
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

// Write log entry (thread-safe)
void MyLogger::write_log(const std::string& level, const std::string& message) {
    std::lock_guard<std::mutex> lock(mtx);
    
    if (!is_open || !log_file.is_open()) {
        std::cerr << "Error: Log file is not open!" << std::endl;
        return;
    }
    
    // Format: [TIMESTAMP] [LEVEL] Message
    log_file << "[" << get_timestamp() << "] "
             << "[" << std::setw(7) << std::left << level << "] "
             << message << std::endl;
}

// Initialize global logger
void init_logger(const std::string& filename) {
    g_logger = std::make_shared<MyLogger>(filename);
}

// Helper function: log info
void log_info(const std::string& message) {
    if (g_logger) {
        g_logger->info(message);
    } else {
        std::cerr << "Warning: Logger not initialized. Call init_logger() first." << std::endl;
    }
}

// Helper function: log warning
void log_warning(const std::string& message) {
    if (g_logger) {
        g_logger->warning(message);
    } else {
        std::cerr << "Warning: Logger not initialized. Call init_logger() first." << std::endl;
    }
}

// Helper function: log error
void log_error(const std::string& message) {
    if (g_logger) {
        g_logger->error(message);
    } else {
        std::cerr << "Warning: Logger not initialized. Call init_logger() first." << std::endl;
    }
}

// Helper function: log debug
void log_debug(const std::string& message) {
    if (g_logger) {
        g_logger->debug(message);
    } else {
        std::cerr << "Warning: Logger not initialized. Call init_logger() first." << std::endl;
    }
}
