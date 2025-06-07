#ifndef COMMON_LOGGER_H
#define COMMON_LOGGER_H

#include <chrono> 
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>

namespace chat_app {
namespace common {

enum class LogLevel { DEBUG, INFO, WARNING, ERROR, CRITICAL };

class Logger {
public:
  static Logger &get_instance() {
    static Logger logger_instance;
    return logger_instance;
  }

  void set_level(LogLevel level) { level_ = level; }

  template <typename... Args>
  void log(LogLevel level, const std::string &component, const std::string &message, Args... args) {
    if (level >= level_) {
      std::lock_guard<std::mutex> lock(mutex_);
      std::ostringstream oss;

      auto now = std::chrono::system_clock::now();
      auto in_time_t = std::chrono::system_clock::to_time_t(now);

      oss << "[" << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %H:%M:%S") << "] ";
      oss << "[" << level_to_string(level) << "] ";
      oss << "[" << component << "] ";

      format_message(oss, message, args...);

      if (level >= LogLevel::ERROR) {
        std::cerr << oss.str() << std::endl;
      } else {
        std::cout << oss.str() << std::endl;
      }
    }
  }

private:
  Logger() : level_(LogLevel::INFO) {}
  ~Logger() = default;
  Logger(const Logger &) = delete;
  Logger &operator=(const Logger &) = delete;

  std::string level_to_string(LogLevel level) {
    switch (level) {
    case LogLevel::DEBUG:
      return "DEBUG";
    case LogLevel::INFO:
      return "INFO";
    case LogLevel::WARNING:
      return "WARNING";
    case LogLevel::ERROR:
      return "ERROR";
    case LogLevel::CRITICAL:
      return "CRITICAL";
    default:
      return "UNKNOWN";
    }
  }

  void format_message(std::ostringstream &oss, const std::string &message) { oss << message; }

  template <typename T, typename... Args>
  void format_message(std::ostringstream &oss, const std::string &format_str, T value, Args... args) {
    size_t placeholder_pos = format_str.find("{}");
    if (placeholder_pos != std::string::npos) {
      oss << format_str.substr(0, placeholder_pos);
      oss << value;
      format_message(oss, format_str.substr(placeholder_pos + 2), args...);
    } else {
      oss << format_str;
    }
  }

  LogLevel level_;
  std::mutex mutex_;
};

// Macros for easier logging
#define LOG_DEBUG(component, ...)                                                                                      \
  chat_app::common::Logger::get_instance().log(chat_app::common::LogLevel::DEBUG, component, __VA_ARGS__)
#define LOG_INFO(component, ...)                                                                                       \
  chat_app::common::Logger::get_instance().log(chat_app::common::LogLevel::INFO, component, __VA_ARGS__)
#define LOG_WARNING(component, ...)                                                                                    \
  chat_app::common::Logger::get_instance().log(chat_app::common::LogLevel::WARNING, component, __VA_ARGS__)
#define LOG_ERROR(component, ...)                                                                                      \
  chat_app::common::Logger::get_instance().log(chat_app::common::LogLevel::ERROR, component, __VA_ARGS__)
#define LOG_CRITICAL(component, ...)                                                                                   \
  chat_app::common::Logger::get_instance().log(chat_app::common::LogLevel::CRITICAL, component, __VA_ARGS__)

} // namespace common
} // namespace chat_app

#endif // COMMON_LOGGER_H