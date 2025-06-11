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

#define CHAT_CLIENT_COMPONENT "ChatClient"

class ChatClient {
public:
  explicit ChatClient(const std::string &username, std::unique_ptr<ServerConnection> server_connection);
  ~ChatClient() = default;

  bool connect_and_join(const std::string &server_address, int server_port);
  void run_user_input_handler();
  void on_message_received(const common::Message &message);

  uint32_t get_user_id() const { return user_id_; }
  const std::string &get_username() const { return username_; }
  const std::unordered_map<uint32_t, std::string> &get_user_map() const { return user_map_; }
  bool is_running() const { return is_running_; }

private:
  void send_join_request();
  void process_join_success(const common::Message &message);
  void process_join_failure(const common::Message &message);
  void process_user_joined(const common::Message &message);
  void process_user_left(const common::Message &message);
  void process_chat_message(const common::Message &message);
  void process_user_joined_list(const common::Message &message);

  // Helper function to get user ID by username
  std::optional<uint32_t> get_user_id_by_name(const std::string &username);

  std::unique_ptr<ServerConnection> server_connection_;
  std::string username_;
  std::atomic<bool> is_running_{true};
  uint32_t user_id_{0};
  std::unordered_map<uint32_t, std::string> user_map_;
  std::mutex count_mutex_;
};

} // namespace client
} // namespace chat_app

#endif // CLIENT_CHAT_CLIENT_H