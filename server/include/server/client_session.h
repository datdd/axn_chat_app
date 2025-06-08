#ifndef SERVER_CLIENT_SESSION_H
#define SERVER_CLIENT_SESSION_H

#include "common/protocol.h"
#include "common/socket.h"
#include <memory>
#include <string>
#include <vector>
#include <cstdint>

namespace chat_app {
namespace server {

class ClientSession {
public:
  ClientSession(uint32_t id, std::unique_ptr<common::IStreamSocket> socket);

  uint32_t get_id() const { return id_; }
  int get_fd() const { return socket_->get_fd(); }
  const std::string &get_username() const { return username_; }
  bool is_authenticated() const { return is_authenticated_; }

  void set_username(const std::string &username) { username_ = std::move(username); }
  void set_authenticated(bool authenticated) { is_authenticated_ = authenticated; }

  common::IStreamSocket *get_socket() const { return socket_.get(); }
  std::vector<char>& get_read_buffer() { return read_buffer_; }

private:
  uint32_t id_;
  std::unique_ptr<common::IStreamSocket> socket_;
  std::string username_;
  bool is_authenticated_{false};
  std::vector<char> read_buffer_;
};

} // namespace server
} // namespace chat_app

#endif // SERVER_CLIENT_SESSION_H