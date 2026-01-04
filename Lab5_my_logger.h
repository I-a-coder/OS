// my_logger.h
// Custom Logger Library Header
// Simple thread-safe logger implementation

#ifndef MY_LOGGER_H
#define MY_LOGGER_H

#include <string>
#include <fstream>
#include <mutex>
#include <memory>

class MyLogger {
private:
    std::ofstream log_file;
    std::mutex mtx;
    std::string filename;
    bool is_open;

public:
    // Constructor
    MyLogger(const std::string& file = "my_logs.txt");
    
    // Destructor
    ~MyLogger();
    
    // Open log file
    bool open(const std::string& file);
    
    // Close log file
    void close();
    
    // Log functions
    void info(const std::string& message);
    void warning(const std::string& message);
    void error(const std::string& message);
    void debug(const std::string& message);
    
    // Flush logs to disk
    void flush();
    
private:
    // Get current timestamp
    std::string get_timestamp();
    
    // Write log entry
    void write_log(const std::string& level, const std::string& message);
};

// Global logger instance
extern std::shared_ptr<MyLogger> g_logger;

// Helper functions for easy logging
void log_info(const std::string& message);
void log_warning(const std::string& message);
void log_error(const std::string& message);
void log_debug(const std::string& message);

// Initialize global logger
void init_logger(const std::string& filename = "my_logs.txt");

#endif // MY_LOGGER_H
