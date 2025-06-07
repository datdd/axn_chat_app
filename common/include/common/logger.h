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
  static Logger &getInstance() {
    static Logger instance;
    return instance;
  }

  void set_level(LogLevel level) { log_level_ = level; }

  template <typename... Args>
  void log(LogLevel level, const std::string component, const std::string &message, Args... args) {
    if (level >= log_level_) {
      std::lock_guard<std::mutex> lock(mutex_);
      std::ostringstream oss;

      auto now = std::chrono::system_clock::now();
      auto in_time_t = std::chrono::system_clock::to_time_t(now);

      oss << "[" << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %H:%M:%S") << "] ";
      oss << "[" << level_to_string(level) << "] ";
      oss << "[" << component << "] ";

      format_message(oss, message, args...);
    }
  }

private:
  Logger() : log_level_(LogLevel::DEBUG) {}

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

  void format_message(std::ostringstream &oss, const std::string &message) {
    oss << message;
  }

  template <typename T, typename... Args>
  void format_message(std::ostringstream &oss, const std::string &format_str, T value, Args... args) {
    size_t pos = format_str.find("{}");
    if (pos != std::string::npos) {
      oss << format_str.substr(0, pos);
      oss << value;
      format_message(oss, format_str.substr(pos + 2), args...);
    } else {
      oss << format_str;
    }
  }

  LogLevel log_level_;
  std::mutex mutex_;
};


// Convenience macros for logging
#define LOG_DEBUG(component, message, ...) \
  chat_app::common::Logger::getInstance().log(chat_app::common::LogLevel::DEBUG, component, message, ##__VA_ARGS__)
#define LOG_INFO(component, message, ...) \
  chat_app::common::Logger::getInstance().log(chat_app::common::LogLevel::INFO, component, message, ##__VA_ARGS__)
#define LOG_WARNING(component, message, ...) \
  chat_app::common::Logger::getInstance().log(chat_app::common::LogLevel::WARNING, component, message, ##__VA_ARGS__)
#define LOG_ERROR(component, message, ...) \
  chat_app::common::Logger::getInstance().log(chat_app::common::LogLevel::ERROR, component, message, ##__VA_ARGS__)
#define LOG_CRITICAL(component, message, ...) \
  chat_app::common::Logger::getInstance().log(chat_app::common::LogLevel::CRITICAL, component, message, ##__VA_ARGS__)

} // namespace common
} // namespace chat_app