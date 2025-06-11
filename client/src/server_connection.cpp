#include "client/server_connection.h"
#include "common/logger.h"

namespace chat_app {
namespace client {

/**
 * @brief Constructor for ServerConnection.
 */
ServerConnection::ServerConnection() = default;

/**
 * @brief Destructor for ServerConnection.
 */
ServerConnection::~ServerConnection() { disconnect(); }

/**
 * @brief Connects to the server at the specified host and port.
 *
 * @param host The hostname or IP address of the server.
 * @param port The port number on which the server is listening.
 * @return true if the connection was successful, false otherwise.
 */
bool ServerConnection::connect(const std::string &host, int port) {
  LOG_INFO(SERVER_CONNECTION_COMPONENT, "Attempting to connect to server at {}:{}", host, port);
  socket_ = common::PosixSocket::create_connector(host, port);

  if (!socket_ || !socket_->is_valid()) {
    LOG_ERROR(SERVER_CONNECTION_COMPONENT, "Failed to connect to server at {}:{}", host, port);
    return false;
  }

  connected_ = true;
  LOG_INFO(SERVER_CONNECTION_COMPONENT, "Successfully connected to server.");
  return true;
}

/**
 * @brief Disconnects from the server and cleans up resources.
 */
void ServerConnection::disconnect() {
  // Set the atomic flag to false. This will signal the receiver_loop to terminate.
  connected_ = false;

  // Close the socket. This will cause any blocking `receive_data` call in the
  // receiver thread to immediately unblock and return an error or 0.
  if (socket_) {
    socket_->close_socket();
  }

  // If the receiver thread is running, we must wait for it to finish execution.
  if (receiver_thread_.joinable()) {
    LOG_DEBUG(SERVER_CONNECTION_COMPONENT, "Waiting for receiver thread to finish...");
    receiver_thread_.join();
  }
}

/**
 * @brief Sends a message to the server.
 *
 * This function serializes the provided message and sends it over the established
 * socket connection. If the connection is not active, it logs a warning and does not
 * attempt to send the message.
 *
 * @param msg The message to be sent.
 */
void ServerConnection::send_message(const common::Message &msg) {
  if (!is_connected()) {
    LOG_WARNING(SERVER_CONNECTION_COMPONENT, "Not connected. Cannot send message.");
    return;
  }

  auto serialized_msg = common::serialize_message(msg);
  auto result = socket_->send_data(serialized_msg);

  if (result.status != common::SocketStatus::OK) {
    LOG_ERROR(SERVER_CONNECTION_COMPONENT, "Failed to send message. Disconnecting.");
    disconnect();
  }
}

/**
 * @brief Starts the receiver thread that listens for incoming messages from the server.
 *
 * This function spawns a new thread that runs the `receiver_loop` method, which
 * continuously reads data from the socket and processes complete messages using
 * the provided callback function `on_message`.
 *
 * @param on_message A callback function that will be called with each deserialized message.
 */
void ServerConnection::start_receiving(const std::function<void(const common::Message &)> &on_message) {
  receiver_thread_ = std::thread(&ServerConnection::receiver_loop, this, on_message);
}

/**
 * @brief Checks if the connection to the server is currently active.
 *
 * @return true if connected, false otherwise.
 */
bool ServerConnection::is_connected() const { return connected_; }

/**
 * @brief The receiver loop runs in a separate thread and continuously reads data
 * from the socket. It deserializes messages and calls the provided callback
 * function `on_message` for each complete message received.
 *
 * @param on_message A callback function that will be called with each
 *                  deserialized message.
 */
void ServerConnection::receiver_loop(const std::function<void(const common::Message &)> &on_message) {
  LOG_DEBUG(SERVER_CONNECTION_COMPONENT, "Receiver thread started.");
  std::vector<char> temp_buffer(4096);

  while (is_connected()) {
    auto result = socket_->receive_data(temp_buffer);

    if (result.status == common::SocketStatus::OK) {
      receive_buffer_.insert(receive_buffer_.end(), temp_buffer.begin(),
                             temp_buffer.begin() + result.bytes_transferred);
    } else if (result.status == common::SocketStatus::CLOSED || result.status == common::SocketStatus::ERROR) {
      LOG_INFO(SERVER_CONNECTION_COMPONENT, "Connection closed by server or error occurred. Shutting down receiver thread.");
      connected_ = false;
      break;
    }

    while (true) {
      auto [msg_opt, bytes_consumed] = common::deserialize_message(receive_buffer_);

      if (msg_opt) {
        on_message(*msg_opt);
        receive_buffer_.erase(receive_buffer_.begin(), receive_buffer_.begin() + bytes_consumed);
      } else {
        break;
      }
    }
  }

  // After the loop terminates, the connection is gone. Send one final, special
  // notification to the ChatClient so it can terminate its user input loop.
  common::Message shutdown_msg;
  shutdown_msg.header.type = common::MessageType::S2C_USER_LEFT;
  shutdown_msg.header.sender_id = common::SERVER_ID;
  on_message(shutdown_msg);

  LOG_INFO(SERVER_CONNECTION_COMPONENT, "Receiver thread terminated.");
}

} // namespace client
} // namespace chat_app