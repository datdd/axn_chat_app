#include "client/server_connection.h"
#include "common/logger.h"

namespace chat_app {
namespace client {

ServerConnection::ServerConnection() = default;
ServerConnection::~ServerConnection() { disconnect(); }

bool ServerConnection::connect(const std::string &host, int port) {
  socket_ = common::PosixSocket::create_connector(host, port);
  if (!socket_ || !socket_->is_valid()) {
    LOG_ERROR("ServerConnection", "Failed to connect to server at {}:{}", host, port);
    return false;
  }
  connected_ = true;

  LOG_INFO("ServerConnection", "Connected to server at {}:{}", host, port);
  return true;
}

void ServerConnection::disconnect() {
  connected_ = false;
  if (socket_) {
    socket_->close_socket();
  }
  if (receive_thread_.joinable()) {
    receive_thread_.join();
  }
}

void ServerConnection::send_message(const common::Message &message) {
  if (!connected_) {
    LOG_ERROR("ServerConnection", "Cannot send message, not connected to server.");
    return;
  }

  auto serialized_message = common::serialize_message(message);
  auto result = socket_->send_data(serialized_message);

  if (result.status != common::SocketStatus::OK) {
    LOG_ERROR("ServerConnection", "Failed to send message: {}. Disconnecting.", result.status);
    disconnect();
  }
}

void ServerConnection::start_receiving(std::function<void(const common::Message &)> message_handler) {
  if (!connected_) {
    LOG_ERROR("ServerConnection", "Cannot start receiving messages, not connected to server.");
    return;
  }

  receive_thread_ = std::thread(&ServerConnection::receive_loop, this, message_handler);
}

void ServerConnection::receive_loop(std::function<void(const common::Message &)> message_handler) {
  std::vector<char> buffer(4096);

  while (is_connected()) {
    auto result = socket_->receive_data(buffer);

    if (result.status == common::SocketStatus::OK) {
      receive_buffer_.insert(receive_buffer_.end(), buffer.begin(), buffer.begin() + result.bytes_transferred);
    } else if (result.status == common::SocketStatus::CLOSED || result.status == common::SocketStatus::ERROR) {
      LOG_ERROR("ServerConnection", "Socket error or closed. Disconnecting.");
      connected_ = false;
      break;
    }

    while (true) {
      auto [message, byte_consumed] = common::deserialize_message(receive_buffer_);

      if (!message) {
        break; // No complete message available
      }
      message_handler(*message);
      // Remove the processed message from the buffer
      receive_buffer_.erase(receive_buffer_.begin(), receive_buffer_.begin() + byte_consumed);
    }
  }

  // Notify the main thread that the connection is closed
  common::Message shutdown_message;
  shutdown_message.header.type = common::MessageType::S2C_USER_LEFT;
  shutdown_message.header.sender_id = common::SERVER_ID;
  message_handler(shutdown_message);

  LOG_INFO("ServerConnection", "Receive loop ended, connection closed.");
}

} // namespace client
} // namespace chat_app