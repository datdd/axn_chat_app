#include "client/chat_client.h"
#include "common/logger.h"
#include <iostream>

int main(int argc, char *argv[]) {
  chat_app::common::Logger::get_instance().set_level(chat_app::common::LogLevel::DEBUG);

  if (argc != 4) {
    std::cerr << "Usage: " << argv[0] << " <host_ip> <port> <username>" << std::endl;
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

  chat_app::client::ChatClient client(username);
  client.run(host, port);

  return 0;
}