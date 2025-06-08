#ifndef SERVER_CLIENT_MANAGER_H
#define SERVER_CLIENT_MANAGER_H

#include "client_session.h"
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace chat_app {
namespace server {

#define CLIENT_MANAGER_COMPONENT "ClientManager"

class ClientManager {
public:
  ClientManager();
  ~ClientManager() = default;

  ClientSession *add_client(std::unique_ptr<common::IStreamSocket> socket);
  void remove_client(int fd);

  ClientSession *get_client_by_id(uint32_t id);
  ClientSession *get_client_by_fd(int fd);
  bool is_username_taken(const std::string &username) const;

  void broadcast_message(const common::Message &message, uint32_t exclude_sender_id);

private:
  uint32_t next_client_id_{1}; // Start from 1 to avoid confusion with SERVER_ID
  std::unordered_map<int, std::unique_ptr<ClientSession>> session_by_fd_;
  std::unordered_map<uint32_t, ClientSession *> session_by_id_;
  std::unordered_set<std::string> usernames_;
};

} // namespace server
} // namespace chat_app

#endif // SERVER_CLIENT_MANAGER_H