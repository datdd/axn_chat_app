#include "client/chat_client.h"
#include "common/logger.h"

#include <arpa/inet.h>
#include <cstring>
#include <iostream>

namespace chat_app {
namespace client {

ChatClient::ChatClient(const std::string &username) : username_(std::move(username)) {}

void ChatClient::run(const std::string &server_address, int server_port) {
  if (!server_connection_.connect(server_address, server_port)) {
    is_running_ = false;
    return;
  }
  is_running_ = true;

  // Send JOIN message request
  common::Message join_message;
  join_message.header.type = common::MessageType::C2S_JOIN;
  join_message.payload.assign(username_.begin(), username_.end());
  join_message.header.payload_size = static_cast<uint32_t>(join_message.payload.size());
  server_connection_.send_message(join_message);

  // Start receiver thread
  server_connection_.start_receiving([this](const common::Message &message) { this->message_handler(message); });

  // Start processing user input in the main thread
  user_input_handler();

  // Cleanup
  server_connection_.disconnect();
  LOG_INFO("ChatClient", "Client has been shut down.");
}

void ChatClient::user_input_handler() {
  std::string input;

  while (is_running_) {
    std::getline(std::cin, input);

    if (input == "/exit") {
      is_running_ = false;
      break;
    }

    if (!input.empty()) {
      common::Message message;
      if (input[0] == '@') { // Private message
        size_t space_pos = input.find(' ');
        if (space_pos == std::string::npos) {
          LOG_ERROR("ChatClient", "Invalid private message format. Use @username message.");
          continue;
        }
        std::string recipient = input.substr(1, space_pos - 1);
        std::string payload = input.substr(space_pos + 1);

        auto user_id = get_user_id_by_name(recipient);
        if (!user_id) {
          LOG_ERROR("ChatClient", "User '{}' not found.", recipient);
          continue;
        }

        message.header.type = common::MessageType::C2S_PRIVATE;
        message.header.receiver_id = user_id.value();
        message.payload.assign(payload.begin(), payload.end());
      } else { // Broadcast message
        message.header.type = common::MessageType::C2S_BROADCAST;
        message.payload.assign(input.begin(), input.end());
        message.header.receiver_id = 0; // Broadcast
      }

      message.header.sender_id = user_id_;
      message.header.payload_size = static_cast<uint32_t>(message.payload.size());

      server_connection_.send_message(message);
    }
  }
}

std::optional<uint32_t> ChatClient::get_user_id_by_name(const std::string &username) {
  std::lock_guard<std::mutex> lock(count_mutex_);
  for (const auto &pair : user_map_) {
    if (pair.second == username) {
      return pair.first;
    }
  }
  return std::nullopt;
}

void ChatClient::message_handler(const common::Message &message) {
  switch (message.header.type) {
  case common::MessageType::S2C_JOIN_SUCCESS: {
    user_id_ = message.header.receiver_id;
    std::string welcome_msg(message.payload.begin(), message.payload.end());
    user_map_[user_id_] = username_;

    std::cout << "[Server]: " << welcome_msg << " (Your ID: " << user_id_ << ")" << std::endl;
    break;
  }
  case common::MessageType::S2C_JOIN_FAILURE: {
    std::string error_msg(message.payload.begin(), message.payload.end());
    is_running_ = false;

    std::cerr << "[Server Error]: " << error_msg << std::endl;
    break;
  }

  case common::MessageType::S2C_USER_JOINED: {
    std::string new_user(message.payload.begin(), message.payload.end());
    std::lock_guard<std::mutex> lock(count_mutex_);
    user_map_[message.header.sender_id] = new_user;

    std::cout << "[Server]: User '" << new_user << "' has joined the chat." << std::endl;
    break;
  }

  case common::MessageType::S2C_USER_LEFT: {
    if (message.header.sender_id == common::SERVER_ID) {
      std::cout << "You have left the chat." << std::endl;
      is_running_ = false;
      break;
    }

    std::string left_user(message.payload.begin(), message.payload.end());
    std::lock_guard<std::mutex> lock(count_mutex_);
    user_map_.erase(message.header.sender_id);

    std::cout << "[Server]: User '" << left_user << "' has left the chat." << std::endl;
    break;
  }

  case common::MessageType::S2C_BROADCAST:
  case common::MessageType::S2C_PRIVATE: {
    std::string sender_name =
        user_map_.count(message.header.sender_id) ? user_map_[message.header.sender_id] : "Unknown";
    std::string payload(message.payload.begin(), message.payload.end());

    std::cout << "[" << sender_name << "]: " << payload << std::endl;
    break;
  }

  case common::MessageType::S2C_ERROR:
    LOG_ERROR("ChatClient", "Error from server: {}", std::string(message.payload.begin(), message.payload.end()));

    break;

  default:
    LOG_WARNING("ChatClient", "Received unknown message type: {}", static_cast<int>(message.header.type));
  }
}

} // namespace client
} // namespace chat_app