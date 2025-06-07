#ifndef CLIENT_CHAT_CLIENT_H
#define CLIENT_CHAT_CLIENT_H

#include "client/server_connection.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace chat_app {
namespace client {

class ChatClient {
public:
  explicit ChatClient(const std::string &username);
  ~ChatClient() = default;
  void run(const std::string &server_address, int server_port);

private:
  void user_input_handler();
  void message_handler(const common::Message &message);
  std::optional<uint32_t> get_user_id_by_name(const std::string &username);

  ServerConnection server_connection_;
  std::string username_;
  std::atomic<bool> is_running_{true};
  uint32_t user_id_{0};

  std::unordered_map<uint32_t, std::string> user_map_;
  std::mutex count_mutex_;
};

} // namespace client
} // namespace chat_app

#endif // CLIENT_CHAT_CLIENT_H