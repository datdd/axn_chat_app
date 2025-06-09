#include "common/logger.h"
#include "server/server.h"
#include <iostream>

int main(int argc, char *argv[]) {
  chat_app::common::Logger::get_instance().set_level(chat_app::common::LogLevel::DEBUG);

  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <port>" << std::endl;
    return 1;
  }

  int port;
  try {
    port = std::stoi(argv[1]);
  } catch (const std::exception &e) {
    std::cerr << "Error: Invalid port number: " << e.what() << std::endl;
    return 1;
  }

  if (port <= 0 || port > 65535) {
    std::cerr << "Error: Port number must be between 1 and 65535." << std::endl;
    return 1;
  }

  chat_app::server::Server server(port);
  server.run();

  return 0;
}