#ifndef CLIENT_SERVER_CONNECTION_H
#define CLIENT_SERVER_CONNECTION_H

#include "common/protocol.h"
#include "common/socket.h"

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <thread>

namespace chat_app {
namespace client {

class ServerConnection {
public:
  ServerConnection();
  ~ServerConnection();

  ServerConnection(const ServerConnection &) = delete;
  ServerConnection &operator=(const ServerConnection &) = delete;

  bool connect(const std::string &host, int port);
  void disconnect();
  void send_message(const common::Message &message);
  void start_receiving(std::function<void(const common::Message &)> message_handler);
  bool is_connected() const { return connected_; }

private:
  void receive_loop(std::function<void(const common::Message &)> message_handler);

  std::unique_ptr<common::IStreamSocket> socket_;
  std::thread receive_thread_;
  std::atomic<bool> connected_{false};
  std::vector<char> receive_buffer_;
};

} // namespace client
} // namespace chat_app

#endif // CLIENT_SERVER_CONNECTION_H