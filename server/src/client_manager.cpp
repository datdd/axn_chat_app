#include "server/client_manager.h"
#include "common/logger.h"
#include "common/protocol.h"
#include "common/socket.h"

namespace chat_app {
namespace server {
ClientManager::ClientManager() = default;

ClientSession *ClientManager::add_client(std::unique_ptr<common::IStreamSocket> socket) {
  int fd = socket->get_fd();
  uint32_t id = next_client_id_++;

  auto session = std::make_unique<ClientSession>(id, std::move(socket));
  auto session_ptr = session.get();

  session_by_fd_[fd] = std::move(session);
  session_by_id_[id] = session_ptr;

  LOG_INFO(CLIENT_MANAGER_COMPONENT, "Client added: ID = {}, FD = {}", id, fd);
  return session_ptr;
}

void ClientManager::remove_client(int fd) {
  auto it = session_by_fd_.find(fd);
  if (it != session_by_fd_.end()) {
    ClientSession *session = it->second.get();
    session_by_id_.erase(session->get_id());
    usernames_.erase(session->get_username());
    session_by_fd_.erase(it);
    LOG_INFO(CLIENT_MANAGER_COMPONENT, "Client removed: ID = {}, FD = {}", session->get_id(), fd);
  } else {
    LOG_WARNING(CLIENT_MANAGER_COMPONENT, "Attempted to remove non-existent client with FD = {}", fd);
  }
}

ClientSession *ClientManager::get_client_by_id(uint32_t id) {
  auto it = session_by_id_.find(id);
  if (it != session_by_id_.end()) {
    return it->second;
  }
  return nullptr;
}

ClientSession *ClientManager::get_client_by_fd(int fd) {
  auto it = session_by_fd_.find(fd);
  if (it != session_by_fd_.end()) {
    return it->second.get();
  }
  return nullptr;
}

bool ClientManager::is_username_taken(const std::string &username) const {
  return usernames_.find(username) != usernames_.end();
}

void ClientManager::broadcast_message(const common::Message &message, uint32_t exclude_sender_id) {
  auto serialize_message = common::serialize_message(message);

  for (auto const &[fd, session] : session_by_fd_) {
    if (session->is_authenticated() && session->get_id() != exclude_sender_id) {
      auto socket = session->get_socket();
      if (socket) {
        socket->send_data(serialize_message);
      } else {
        LOG_ERROR(CLIENT_MANAGER_COMPONENT, "Socket for client ID {} is null", session->get_id());
      }
    }
  }
}

} // namespace server
} // namespace chat_app