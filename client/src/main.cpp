#include "client/chat_client.h"
#include "common/logger.h"
#include <iostream>

void show_help() {
  std::cout << "Usage: chat_client <host_ip> <port> <username>\n"
            << "  host_ip   - The IP address of the chat server.\n"
            << "  port      - The listening port number of the chat server.\n"
            << "  username  - Your username for the chat.\n";
}

void show_send_private_message_guide() {
  std::cout << "To send a private message, type '@<username> <message>'\n"
            << "Example: '@john Hello, how are you?'\n";
}

int main(int argc, char *argv[]) {
  chat_app::common::Logger::get_instance().set_level(chat_app::common::LogLevel::INFO);

  if (argc != 4) {
    show_help();
    return 1;
  }

  std::string host = argv[1];
  int port;
  try {
    port = std::stoi(argv[2]);
  } catch (const std::invalid_argument &e) {
    std::cerr << "Error: Invalid port number." << std::endl;
    return 1;
  } catch (const std::out_of_range &e) {
    std::cerr << "Error: Port number out of range." << std::endl;
    return 1;
  }
  std::string username = argv[3];

  if (username.empty() || username.length() > 32) {
    std::cerr << "Error: Username must be between 1 and 32 characters." << std::endl;
    return 1;
  }

  LOG_INFO("Main", "Starting client for user '{}' connecting to {}:{}", username, host, port);

  // Create a server connection
  auto server_connection = std::make_unique<chat_app::client::ServerConnection>();
  chat_app::client::ChatClient client(username, std::move(server_connection));
  
  if (client.connect_and_join(host, port)) {
    show_send_private_message_guide();
    client.request_list_of_users();
    client.run_user_input_handler();
  }

  return 0;
}