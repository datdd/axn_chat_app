#include "common/socket.h"
#include "common/logger.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

namespace chat_app {
namespace common {

/**
 * @brief Constructor for PosixSocket.
 * The socket is created in the AF_INET domain (IPv4), with a SOCK_STREAM type (TCP).
 */
PosixSocket::PosixSocket() : socket_fd_(::socket(AF_INET, SOCK_STREAM, 0)) {}

/**
 * @brief Constructor for PosixSocket with an existing file descriptor.
 * This constructor is used to wrap an existing socket file descriptor.
 * @param fd The file descriptor of the existing socket.
 */
PosixSocket::PosixSocket(int fd) : socket_fd_(fd) {}

/**
 * @brief Destructor for PosixSocket.
 * Closes the socket file descriptor if it is valid.
 */
PosixSocket::~PosixSocket() { close_socket(); }

/**
 * @brief Closes the socket file descriptor.
 */
void PosixSocket::close_socket() {
  if (is_valid()) {
    ::close(socket_fd_);
    socket_fd_ = -1;
  }
}

/**
 * @brief Checks if the socket is valid.
 * A socket is considered valid if its file descriptor is greater than or equal to 0.
 * @return True if the socket is valid, false otherwise.
 */
bool PosixSocket::is_valid() const { return socket_fd_ >= 0; }

/**
 * @brief Gets the file descriptor of the socket.
 * This method returns the file descriptor associated with the socket.
 * @return The file descriptor of the socket.
 */
int PosixSocket::get_fd() const { return socket_fd_; }

/**
 * @brief Sets the socket to non-blocking mode or blocking mode.
 * This method modifies the socket's file descriptor flags to set it as non-blocking or blocking.
 * @param non_blocking If true, sets the socket to non-blocking mode; otherwise, sets it to blocking mode.
 */
void PosixSocket::set_non_blocking(bool non_blocking) {
  int flags = fcntl(socket_fd_, F_GETFL, 0);
  if (flags == -1) {
    LOG_ERROR("PosixSocket", "Failed to get socket flags: {}", strerror(errno));
    return;
  }

  if (non_blocking) {
    flags |= O_NONBLOCK;
  } else {
    flags &= ~O_NONBLOCK;
  }

  if (fcntl(socket_fd_, F_SETFL, flags) == -1) {
    LOG_ERROR("PosixSocket", "Failed to set socket flags: {}", strerror(errno));
  }
}

std::unique_ptr<IListeningSocket> PosixSocket::create_listener() {
  auto sock = std::unique_ptr<PosixSocket>(new PosixSocket());
  
  if (!sock->is_valid()) {
    LOG_CRITICAL("Socket", "Failed to create listener socket: {}", strerror(errno));
    return nullptr;
  }
  
  return sock;
}

std::unique_ptr<IStreamSocket> PosixSocket::create_connector(const std::string &ip_address, int port) {
  auto sock = std::unique_ptr<PosixSocket>(new PosixSocket());

  if (!sock->is_valid()) {
    LOG_CRITICAL("Socket", "Failed to create connector socket: {}", strerror(errno));
    return nullptr;
  }

  sockaddr_in server_addr{};
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
  if (inet_pton(AF_INET, ip_address.c_str(), &server_addr.sin_addr) <= 0) {
    LOG_CRITICAL("Socket", "Invalid IP address: {}", ip_address);
    return nullptr;
  }

  if (connect(sock->get_fd(), reinterpret_cast<sockaddr *>(&server_addr), sizeof(server_addr)) < 0) {
    LOG_CRITICAL("Socket", "Connection failed: {}", strerror(errno));
    return nullptr;
  }

  return sock;
}

bool PosixSocket::bind_socket(int port) {
  if (!is_valid()) {
    LOG_ERROR("PosixSocket", "Cannot bind an invalid socket");
    return false;
  }

  int opt = 1;
  if (setsockopt(socket_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
    LOG_ERROR("PosixSocket", "Failed to set socket options: {}", strerror(errno));
    return false;
  }

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(port);

  if (bind(socket_fd_, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
    LOG_ERROR("PosixSocket", "Failed to bind socket: {}", strerror(errno));
    return false;
  }

  LOG_INFO("PosixSocket", "Socket bound to port {}", port);
  return true;
}

bool PosixSocket::listen_socket(int backlog) {
  if (!is_valid()) {
    LOG_ERROR("PosixSocket", "Cannot listen on an invalid socket");
    return false;
  }

  if (listen(socket_fd_, backlog) < 0) {
    LOG_ERROR("PosixSocket", "Failed to listen on socket: {}", strerror(errno));
    return false;
  }

  LOG_INFO("PosixSocket", "Socket is now listening with backlog {}", backlog);
  return true;
}

std::unique_ptr<IStreamSocket> PosixSocket::accept_connection() {
  if (!is_valid()) {
    LOG_ERROR("PosixSocket", "Cannot accept on an invalid socket");
    return nullptr;
  }

  int client_fd = accept(socket_fd_, nullptr, nullptr);
  if (client_fd < 0) {
    LOG_ERROR("PosixSocket", "Failed to accept connection: {}", strerror(errno));
    return nullptr;
  }

  LOG_INFO("PosixSocket", "Accepted new connection with fd {}", client_fd);
  return std::make_unique<PosixSocket>(client_fd);
}

SocketResult PosixSocket::send_data(const std::vector<char> &data) {
  if (!is_valid()) {
    LOG_ERROR("PosixSocket", "Cannot send data on an invalid socket");
    return {SocketStatus::ERROR, 0};
  }

  ssize_t bytes_sent = send(socket_fd_, data.data(), data.size(), 0);
  if (bytes_sent < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      LOG_WARNING("PosixSocket", "Send operation would block, no data sent");
      return {SocketStatus::WOULD_BLOCK, 0};
    }
    if (errno == ECONNRESET || errno == EPIPE) {
      LOG_ERROR("PosixSocket", "Connection reset by peer or broken pipe");
      return {SocketStatus::CLOSED, 0};
    }

    LOG_ERROR("PosixSocket", "Failed to send data: {}", strerror(errno));
    return {SocketStatus::ERROR, 0};
  }

  LOG_INFO("PosixSocket", "Sent {} bytes of data", bytes_sent);
  return {SocketStatus::OK, static_cast<std::size_t>(bytes_sent)};
}

SocketResult PosixSocket::receive_data(std::vector<char> &buffer, std::size_t size) {
  if (!is_valid()) {
    LOG_ERROR("PosixSocket", "Cannot receive data on an invalid socket");
    return {SocketStatus::ERROR, 0};
  }

  buffer.resize(size);
  ssize_t bytes_received = recv(socket_fd_, buffer.data(), size, 0);
  if (bytes_received < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      LOG_WARNING("PosixSocket", "Receive operation would block, no data received");
      return {SocketStatus::WOULD_BLOCK, 0};
    }
    if (errno == ECONNRESET || errno == EPIPE) {
      LOG_ERROR("PosixSocket", "Connection reset by peer or broken pipe");
      return {SocketStatus::CLOSED, 0};
    }

    LOG_ERROR("PosixSocket", "Failed to receive data: {}", strerror(errno));
    return {SocketStatus::ERROR, 0};
  }

  buffer.resize(bytes_received);
  LOG_INFO("PosixSocket", "Received {} bytes of data", bytes_received);
  return {SocketStatus::OK, static_cast<std::size_t>(bytes_received)};
}

} // namespace common
} // namespace chat_app