#include "client/chat_client.h"
#include "common/logger.h"

#include <arpa/inet.h>
#include <cstring>
#include <iostream>

namespace chat_app {
namespace client {

ChatClient::ChatClient(const std::string &username, std::unique_ptr<ServerConnection> server_connection)
    : username_(username), server_connection_(std::move(server_connection)), is_running_(false), user_id_(0) {}

bool ChatClient::connect_and_join(const std::string &server_address, int server_port) {
  if (!server_connection_->connect(server_address, server_port)) {
    LOG_ERROR(CHAT_CLIENT_COMPONENT, "Failed to connect to server at {}:{}", server_address, server_port);
    return false;
  }

  // Send join request to the server
  send_join_request();

  // Start receiving messages from the server
  server_connection_->start_receiving([this](const common::Message &message) { this->on_message_received(message); });

  is_running_ = true;
  return true;
}

void ChatClient::run_user_input_handler() {
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
          LOG_ERROR(CHAT_CLIENT_COMPONENT, "Invalid private message format. Use @username message.");
          continue;
        }
        std::string receiver = input.substr(1, space_pos - 1);
        std::string msg_str = input.substr(space_pos + 1);

        auto user_id = get_user_id_by_name(receiver);
        if (!user_id) {
          LOG_ERROR(CHAT_CLIENT_COMPONENT, "User '{}' not found.", receiver);
          continue;
        }

        message = common::Message(common::MessageType::C2S_PRIVATE, user_id_, user_id.value(), msg_str);
      } else { // Broadcast message
        message = common::Message(common::MessageType::C2S_BROADCAST, user_id_, common::BROADCAST_ID, input);
      }

      server_connection_->send_message(message);
    }
  }

  server_connection_->disconnect();
  LOG_INFO(CHAT_CLIENT_COMPONENT, "User input loop terminated. Client is shutting down.");
}

void ChatClient::send_join_request() {
  common::Message join_message(common::MessageType::C2S_JOIN, common::INVALID_ID, common::SERVER_ID, username_);
  server_connection_->send_message(join_message);
}

void ChatClient::on_message_received(const common::Message &message) {
  switch (message.header.type) {
  case common::MessageType::S2C_JOIN_SUCCESS: {
    process_join_success(message);
    break;
  }
  case common::MessageType::S2C_JOIN_FAILURE: {
    process_join_failure(message);
    break;
  }
  case common::MessageType::S2C_USER_JOINED: {
    process_user_joined(message);
    break;
  }
  case common::MessageType::S2C_USER_LEFT: {
    process_user_left(message);
    break;
  }
  case common::MessageType::S2C_BROADCAST:
  case common::MessageType::S2C_PRIVATE: {
    process_chat_message(message);
    break;
  }
  case common::MessageType::S2C_USER_JOINED_LIST: {
    process_user_joined_list(message);
    break;
  }
  case common::MessageType::S2C_ERROR:
    LOG_ERROR(CHAT_CLIENT_COMPONENT, "Error from server: {}",
              std::string(message.payload.begin(), message.payload.end()));
    break;
  default:
    LOG_WARNING(CHAT_CLIENT_COMPONENT, "Received unknown message type: {}", static_cast<int>(message.header.type));
  }
}

void ChatClient::process_join_success(const common::Message &message) {
  user_id_ = message.header.receiver_id;
  std::string welcome_msg(message.payload.begin(), message.payload.end());
  user_map_[user_id_] = username_;

  std::cout << "[Server]: " << welcome_msg << " (Your ID: " << user_id_ << ")" << std::endl;
}

void ChatClient::process_join_failure(const common::Message &message) {
  std::string error_msg(message.payload.begin(), message.payload.end());
  is_running_ = false;

  std::cerr << "[Server Error]: " << error_msg << std::endl;
}

void ChatClient::process_user_joined(const common::Message &message) {
  std::string new_user(message.payload.begin(), message.payload.end());
  std::lock_guard<std::mutex> lock(count_mutex_);
  user_map_[message.header.sender_id] = new_user;

  std::cout << "[Server]: User '" << new_user << "' has joined the chat." << std::endl;
}

void ChatClient::process_user_left(const common::Message &message) {
  if (message.header.sender_id == common::SERVER_ID) {
    std::cout << "You have left the chat." << std::endl;
    is_running_ = false;
    return;
  }

  std::string left_user(message.payload.begin(), message.payload.end());
  std::lock_guard<std::mutex> lock(count_mutex_);
  user_map_.erase(message.header.sender_id);

  std::cout << "[Server]: User '" << left_user << "' has left the chat." << std::endl;
}

void ChatClient::process_chat_message(const common::Message &message) {
  std::string sender_name = user_map_.count(message.header.sender_id) ? user_map_[message.header.sender_id] : "Unknown";
  std::string payload(message.payload.begin(), message.payload.end());

  std::cout << "[" << sender_name << "]: " << payload << std::endl;
}

void ChatClient::process_user_joined_list(const common::Message &message) {
  std::cout << "[Server]: Current users in the chat:" << std::endl;
  std::lock_guard<std::mutex> lock(count_mutex_);

  // Clear previous user map
  // user_map_.clear();

  // Deserialize user list
  size_t pos = 0;
  while (pos < message.payload.size()) {
    size_t next_pos = message.payload.find(',', pos);
    if (next_pos == std::string::npos) {
      next_pos = message.payload.size();
    }
    std::string user_entry = message.payload.substr(pos, next_pos - pos);
    pos = next_pos + 1;

    if (!user_entry.empty()) {
      auto separator_pos = user_entry.find(':');
      if (separator_pos != std::string::npos) {
        std::string username = user_entry.substr(0, separator_pos);
        uint32_t user_id = std::stoul(user_entry.substr(separator_pos + 1));
        user_map_[user_id] = username;
      }
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

} // namespace client
} // namespace chat_app