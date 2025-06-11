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

#define SERVER_CONNECTION_COMPONENT "ServerConnection"

/**
 * @brief The ServerConnection class manages the connection to the chat server.
 * It handles connecting, disconnecting, sending messages, and receiving messages.
 */
class ServerConnection {
public:
  ServerConnection();
  virtual ~ServerConnection();

  ServerConnection(const ServerConnection &) = delete;
  ServerConnection &operator=(const ServerConnection &) = delete;

  virtual bool connect(const std::string &host, int port);
  virtual void disconnect();
  virtual void send_message(const common::Message &msg);
  virtual void start_receiving(const std::function<void(const common::Message &)> &on_message);
  virtual bool is_connected() const;

private:
  void receiver_loop(const std::function<void(const common::Message &)> &on_message);

  std::unique_ptr<common::IStreamSocket> socket_;
  std::thread receiver_thread_;
  std::atomic<bool> connected_{false};
  std::vector<char> receive_buffer_;
};

} // namespace client
} // namespace chat_app

#endif // CLIENT_SERVER_CONNECTION_H