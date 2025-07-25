#ifndef SERVER_SERVER_H
#define SERVER_SERVER_H

#include "server/client_manager.h"
#include "server/epoll_manager.h"
#include <atomic>
#include <memory>

namespace chat_app {
namespace server {

#define SERVER_COMPONENT "Server"

/**
 * @brief The Server class handles the chat server functionality.
 * It listens for incoming connections, manages client sessions, and processes messages.
 */
class Server {
public:
  explicit Server(int port);
  ~Server() = default;

  void run();
  void stop();

private:
  void handle_new_connection();
  void handle_client_message(int fd);
  void handle_client_disconnection(int fd);
  
  void process_message(ClientSession &session, const common::Message &message);
  void process_join_message(ClientSession &session, const common::Message &message);
  void process_user_joined_list(ClientSession &session);
  void process_broadcast_message(ClientSession &session, const common::Message &message);
  void process_private_message(ClientSession &session, const common::Message &message);

  void shutdown();

  int port_;
  std::unique_ptr<common::IListeningSocket> listener_;
  EpollManager epoll_manager_;
  ClientManager client_manager_;
  std::atomic<bool> running_{true};
  int server_event_fd_{-1};
};

} // namespace server
} // namespace chat_app

#endif // SERVER_SERVER_H