#include "server/server.h"
#include "common/logger.h"
#include <arpa/inet.h>
#include <cstring>

namespace chat_app {
namespace server {

Server::Server(int port) : port_(port), epoll_manager_(1024) {}

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
  // Register the listener socket with epoll
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
}

/**
 * @brief Handles a new incoming connection.
 * Accepts the connection, sets it to non-blocking mode, and registers it with epoll.
 */
void Server::handle_new_connection() {
  while(auto client_socket = listener_->accept_connection()) {
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
  auto& read_buffer = session->get_read_buffer();

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
  if (!session) return;

  LOG_INFO(SERVER_COMPONENT, "Client disconnected: ID = {}, FD = {}", session->get_id(), fd);

  if (session->is_authenticated()) {
    common::Message message;
    message.header.type = common::MessageType::S2C_USER_LEFT;
    message.header.sender_id = common::SERVER_ID;
    message.header.receiver_id = common::BROADCAST_ID;
    message.payload.assign(session->get_username().begin(), session->get_username().end());
    message.header.payload_size = static_cast<uint32_t>(message.payload.size());

    client_manager_.broadcast_message(message, session->get_id());
  }

  client_manager_.remove_client(fd);
  epoll_manager_.remove_fd(fd);
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
      // Handle join message
      if (!session.is_authenticated()) {
        std::string username(message.payload.begin(), message.payload.end());
        if (client_manager_.is_username_taken(username)) {
          common::Message response;
          std::string error_message = "Username already taken";
          response.header.type = common::MessageType::S2C_JOIN_FAILURE;
          response.header.sender_id = common::SERVER_ID;
          response.header.receiver_id = session.get_id();
          response.payload.assign(error_message.begin(), error_message.end());
          response.header.payload_size = static_cast<uint32_t>(response.payload.size());
          session.get_socket()->send_data(common::serialize_message(response));

          // Force disconnect
          handle_client_disconnection(session.get_fd());
        } else {
          session.set_username(username);
          session.set_authenticated(true);
          
          common::Message response;
          std::string welcome_message = "Welcome to the chat, " + username + "!";
          response.header.type = common::MessageType::S2C_JOIN_SUCCESS;
          response.header.sender_id = common::SERVER_ID;
          response.header.receiver_id = session.get_id();
          response.payload.assign(welcome_message.begin(), welcome_message.end());
          response.header.payload_size = static_cast<uint32_t>(response.payload.size());
          session.get_socket()->send_data(common::serialize_message(response));

          // Notify other clients about the new user
          common::Message user_joined_message;
          user_joined_message.header.type = common::MessageType::S2C_USER_JOINED;
          user_joined_message.header.sender_id = common::SERVER_ID;
          user_joined_message.header.receiver_id = common::BROADCAST_ID;
          user_joined_message.payload.assign(username.begin(), username.end());
          user_joined_message.header.payload_size = static_cast<uint32_t>(user_joined_message.payload.size());
          client_manager_.broadcast_message(user_joined_message, session.get_id());
          
        }
      }
      break;
    }
    case common::MessageType::C2S_BROADCAST: {
      // Handle broadcast message
      if (session.is_authenticated()) {
        common::Message broadcast_message = message;
        broadcast_message.header.type = common::MessageType::S2C_BROADCAST;
        broadcast_message.header.sender_id = session.get_id();
        broadcast_message.header.receiver_id = common::BROADCAST_ID;
        broadcast_message.payload = message.payload;
        broadcast_message.header.payload_size = message.header.payload_size;
        client_manager_.broadcast_message(broadcast_message, session.get_id());
      }
      break;
    }
    case common::MessageType::C2S_PRIVATE: {
      // Handle private message
      if (session.is_authenticated()) {
        // Implement private messaging logic here
        common::Message private_message = message;
        private_message.header.type = common::MessageType::S2C_PRIVATE;
        private_message.header.sender_id = session.get_id();
        private_message.header.receiver_id = message.header.receiver_id;
        private_message.payload = message.payload;
        private_message.header.payload_size = message.header.payload_size;

        auto receiver_session = client_manager_.get_client_by_id(message.header.receiver_id);
      }
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

} // namespace server
} // namespace chat_app