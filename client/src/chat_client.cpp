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
  send_join_request();
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
        message.header.receiver_id = common::BROADCAST_ID;
        message.payload.assign(input.begin(), input.end());
      }

      message.header.sender_id = user_id_;
      message.header.payload_size = static_cast<uint32_t>(message.payload.size());

      server_connection_.send_message(message);
    }
  }
}

void ChatClient::send_join_request() {
  common::Message join_message;
  join_message.header.type = common::MessageType::C2S_JOIN;
  join_message.header.sender_id = common::INVALID_ID;
  join_message.header.receiver_id = common::SERVER_ID;
  join_message.payload.assign(username_.begin(), username_.end());
  join_message.header.payload_size = static_cast<uint32_t>(join_message.payload.size());
  server_connection_.send_message(join_message);
}

void ChatClient::message_handler(const common::Message &message) {
  switch (message.header.type) {
  case common::MessageType::S2C_JOIN_SUCCESS: {
    process_join_message(message);
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
    LOG_ERROR("ChatClient", "Error from server: {}", std::string(message.payload.begin(), message.payload.end()));
    break;
  default:
    LOG_WARNING("ChatClient", "Received unknown message type: {}", static_cast<int>(message.header.type));
  }
}

void ChatClient::process_join_message(const common::Message &message) {
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
  user_map_.clear();

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