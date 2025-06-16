#include "server/server.h"
#include "common/logger.h"
#include <arpa/inet.h>
#include <cstring>

namespace chat_app {
namespace server {

Server::Server(int port) : port_(port), epoll_manager_(1024) {}

/**
 * @brief Runs the server, listening for incoming connections and handling client messages.
 */
void Server::run() {
  listener_ = common::PosixSocket::create_listener();
  if (!listener_) {
    LOG_ERROR(SERVER_COMPONENT, "Failed to create listening socket on port {}", port_);
    return;
  }

  if (!listener_->bind_socket(port_) || !listener_->listen_socket(1024)) {
    LOG_ERROR(SERVER_COMPONENT, "Failed to bind or listen on port {}", port_);
    return;
  }

  listener_->set_non_blocking(true);
  // Use EPOLLET for edge-triggered mode
  epoll_manager_.add_fd(listener_->get_fd(), EPOLLIN | EPOLLET);

  running_ = true;
  LOG_INFO(SERVER_COMPONENT, "Server started on port {}. Waiting for new connections ...", port_);

  while (running_) {
    int num_events = epoll_manager_.wait(-1);
    if (num_events < 0) {
      LOG_ERROR(SERVER_COMPONENT, "Epoll wait failed: {}", std::strerror(errno));
      continue;
    }
    LOG_DEBUG(SERVER_COMPONENT, "Epoll returned {} events", num_events);

    for (int i = 0; i < num_events; ++i) {
      const auto &event = epoll_manager_.get_events()[i];
      if (event.data.fd == listener_->get_fd()) {
        handle_new_connection();
      } else {
        if ((event.events & EPOLLHUP) || (event.events & EPOLLERR)) {
          handle_client_disconnection(event.data.fd);
        } else if (event.events & EPOLLIN) {
          // Handle incoming messages from clients
          handle_client_message(event.data.fd);
        }
      }
    }
  }

  LOG_INFO(SERVER_COMPONENT, "Server stopped.");
  shutdown();
}

/**
 * @brief Stops the server gracefully.
 */
void Server::stop() {
  // Set the running flag to false to stop the server loop
  running_.store(false);
  // The epoll_wait might be blocking indefinitely. We need to wake it up.
  // A common trick is to make a dummy connection to the server itself.
  auto dummy_socket = common::PosixSocket::create_connector("127.0.0.1", port_);
  if (dummy_socket) {
    dummy_socket->close_socket();
  }
}

/**
 * @brief Shuts down the server.
 * Closes the listener socket and notifies all clients about the shutdown.
 */
void Server::shutdown() {
  LOG_INFO(SERVER_COMPONENT, "Shutting down server...");
  listener_->close_socket();

  // Notify all clients about the server shutdown
  common::Message server_shutdown_message(common::MessageType::S2C_SERVER_SHUTDOWN, common::SERVER_ID,
                                          common::BROADCAST_ID, "Server is shutting down.");
  client_manager_.broadcast_message(server_shutdown_message, common::SERVER_ID);

  LOG_INFO(SERVER_COMPONENT, "Server shutdown complete.");
}

/**
 * @brief Handles a new incoming connection.
 * Accepts the connection, sets it to non-blocking mode, and registers it with epoll.
 */
void Server::handle_new_connection() {
  while (auto client_socket = listener_->accept_connection()) {
    client_socket->set_non_blocking(true);
    int fd = client_socket->get_fd();
    LOG_INFO(SERVER_COMPONENT, "New connection accepted: FD = {}", fd);

    auto session = client_manager_.add_client(std::move(client_socket));
    epoll_manager_.add_fd(fd, EPOLLIN | EPOLLET);
  }
}

/**
 * @brief Handles incoming messages from a client.
 * Reads the message, deserializes it, and processes it.
 *
 * @param fd The file descriptor of the client.
 */
void Server::handle_client_message(int fd) {
  auto session = client_manager_.get_client_by_fd(fd);
  if (!session) {
    LOG_WARNING(SERVER_COMPONENT, "Received message from unknown client with FD = {}", fd);
    return;
  }

  std::vector<char> buffer(4096);
  auto &read_buffer = session->get_read_buffer();

  while (true) {
    auto result = session->get_socket()->receive_data(buffer);
    if (result.status == common::SocketStatus::OK) {
      read_buffer.insert(read_buffer.end(), buffer.begin(), buffer.begin() + result.bytes_transferred);
    } else if (result.status == common::SocketStatus::WOULD_BLOCK) {
      // No more data available right now
      break;
    } else {
      handle_client_disconnection(fd);
      return;
    }
  }

  // Deserialize the message
  while (true) {
    auto [message, bytes_read] = common::deserialize_message(read_buffer);
    if (!message) {
      // Not enough data to deserialize
      break;
    }

    read_buffer.erase(read_buffer.begin(), read_buffer.begin() + bytes_read);
    process_message(*session, *message);
  }
}

/**
 * @brief Handles client disconnection.
 * Removes the client from the manager and unregisters it from epoll.
 * If the client was authenticated, it broadcasts a user left message.
 *
 * @param fd The file descriptor of the client.
 */
void Server::handle_client_disconnection(int fd) {
  auto session = client_manager_.get_client_by_fd(fd);
  if (!session)
    return;

  LOG_INFO(SERVER_COMPONENT, "Client disconnected: ID = {}, FD = {}", session->get_id(), fd);

  if (session->is_authenticated()) {
    common::Message user_left_message(common::MessageType::S2C_USER_LEFT, session->get_id(), common::BROADCAST_ID,
                                      session->get_username());
    client_manager_.broadcast_message(user_left_message, session->get_id());
  }

  epoll_manager_.remove_fd(fd);
  client_manager_.remove_client(fd);
}

/**
 * @brief Processes a message received from a client.
 * Depending on the message type, it performs the appropriate action.
 *
 * @param session The client session that sent the message.
 * @param message The deserialized message.
 */
void Server::process_message(ClientSession &session, const common::Message &message) {
  switch (message.header.type) {
  case common::MessageType::C2S_JOIN: {
    process_join_message(session, message);
    break;
  }
  case common::MessageType::C2S_USER_JOINED_LIST: {
    process_user_joined_list(session);
    break;
  }
  case common::MessageType::C2S_BROADCAST: {
    process_broadcast_message(session, message);
    break;
  }
  case common::MessageType::C2S_PRIVATE: {
    process_private_message(session, message);
    break;
  }
  case common::MessageType::C2S_LEAVE: {
    handle_client_disconnection(session.get_fd());
    break;
  }
  default:
    LOG_WARNING(SERVER_COMPONENT, "Received unknown message type: {}", static_cast<uint8_t>(message.header.type));
  }
}

/**
 * @brief Processes a join message from a client.
 *
 * @param session The client session that sent the join message.
 * @param message The join message containing the username.
 */
void Server::process_join_message(ClientSession &session, const common::Message &message) {
  if (!session.is_authenticated()) {
    std::string username(message.payload.begin(), message.payload.end());
    if (client_manager_.is_username_taken(username)) {
      common::Message user_already_exists_message(common::MessageType::S2C_JOIN_FAILURE, common::SERVER_ID,
                                                  session.get_id(), "Username already exists");
      session.get_socket()->send_data(common::serialize_message(user_already_exists_message));

      LOG_WARNING(SERVER_COMPONENT, "Client with FD {} tried to join with an existing username: {}", session.get_fd(),
                  username);

      // Force disconnect
      handle_client_disconnection(session.get_fd());
    } else {
      session.set_username(username);
      session.set_authenticated(true);
      client_manager_.add_username(username);

      // Send a success message back to the client
      common::Message user_joined_message(common::MessageType::S2C_JOIN_SUCCESS, common::SERVER_ID, session.get_id(),
                                          "Welcome to the chat, " + username + "!");
      session.get_socket()->send_data(common::serialize_message(user_joined_message));

      // Broadcast the user joined message to all other clients
      common::Message notify_user_joined_message(common::MessageType::S2C_USER_JOINED, session.get_id(),
                                                 common::BROADCAST_ID, username);
      client_manager_.broadcast_message(notify_user_joined_message, session.get_id());

      LOG_INFO(SERVER_COMPONENT, "Client with FD {} joined with username: {}", session.get_fd(), username);
    }
  }
}

/**
 * @brief Processes a request for the list of users currently connected to the server.
 *
 * @param session The client session that requested the user list.
 */
void Server::process_user_joined_list(ClientSession &session) {
  std::vector<std::string> user_list;

  for (const auto &client : client_manager_.get_all_clients()) {
    if (client->is_authenticated() && client->get_id() != session.get_id()) {
      user_list.push_back(client->get_username() + ":" + std::to_string(client->get_id()));
    }
  }

  if (!user_list.empty()) {
    std::string user_list_str;
    for (const auto &user : user_list) {
      user_list_str += user + ",";
    }
    user_list_str.pop_back(); // Remove last comma

    common::Message user_list_message(common::MessageType::S2C_USER_JOINED_LIST, common::SERVER_ID, session.get_id(),
                                      user_list_str);
    session.get_socket()->send_data(common::serialize_message(user_list_message));
  }
}

/**
 * @brief Processes a broadcast message from a client.
 *
 * @param session The client session that sent the broadcast message.
 * @param message The broadcast message containing the payload.
 */
void Server::process_broadcast_message(ClientSession &session, const common::Message &message) {
  if (session.is_authenticated()) {
    common::Message broadcast_message(common::MessageType::S2C_BROADCAST, session.get_id(), common::BROADCAST_ID,
                                      message.payload);
    client_manager_.broadcast_message(broadcast_message, session.get_id());
  }
}

/**
 * @brief Processes a private message from a client to another client.
 *
 * @param session The client session that sent the private message.
 * @param message The private message containing the payload and receiver ID.
 */
void Server::process_private_message(ClientSession &session, const common::Message &message) {
  auto receiver_session = client_manager_.get_client_by_id(message.header.receiver_id);
  if (session.is_authenticated() && receiver_session) {
    common::Message private_message(common::MessageType::S2C_PRIVATE, session.get_id(), message.header.receiver_id,
                                    message.payload);
    receiver_session->get_socket()->send_data(common::serialize_message(private_message));
  } else if (!receiver_session) {
    common::Message error_message(common::MessageType::S2C_ERROR, common::SERVER_ID, session.get_id(),
                                  "Receiver not found or not connected.");
    session.get_socket()->send_data(common::serialize_message(error_message));
  }
}

} // namespace server
} // namespace chat_app