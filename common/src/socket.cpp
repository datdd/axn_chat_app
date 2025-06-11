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
PosixSocket::PosixSocket() {
  socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (socket_fd_ < 0) {
    LOG_CRITICAL(COMMON_POSIX_SOCKET_COMPONENT, "Failed to create socket: {}", strerror(errno));
  } else {
    LOG_DEBUG(COMMON_POSIX_SOCKET_COMPONENT, "Socket created with fd {}", socket_fd_);
  }
}

/**
 * @brief Constructor for PosixSocket with an existing file descriptor.
 * This constructor is used to wrap an existing socket file descriptor.
 * @param fd The file descriptor of the existing socket.
 */
PosixSocket::PosixSocket(int fd) : socket_fd_(fd) {
  if (socket_fd_ < 0) {
    LOG_CRITICAL(COMMON_POSIX_SOCKET_COMPONENT, "Invalid socket file descriptor: {}", strerror(errno));
  } else {
    LOG_DEBUG(COMMON_POSIX_SOCKET_COMPONENT, "Socket created with fd {}", socket_fd_);
  }
}

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
 * @return True if the socket is valid, false otherwise.
 */
bool PosixSocket::is_valid() const { return socket_fd_ != -1; }

/**
 * @brief Gets the file descriptor of the socket.
 * @return The file descriptor of the socket.
 */
int PosixSocket::get_fd() const { return socket_fd_; }

/**
 * @brief Sets the socket to non-blocking mode or blocking mode.
 * @param non_blocking If true, sets the socket to non-blocking mode; otherwise, sets it to blocking mode.
 */
void PosixSocket::set_non_blocking(bool non_blocking) {
  if (!is_valid())
    return;

  int flags = fcntl(socket_fd_, F_GETFL, 0);
  if (flags == -1) {
    LOG_ERROR(COMMON_POSIX_SOCKET_COMPONENT, "Failed to get socket flags: {}", strerror(errno));
    return;
  }

  if (non_blocking) {
    flags |= O_NONBLOCK;
  } else {
    flags &= ~O_NONBLOCK;
  }

  if (fcntl(socket_fd_, F_SETFL, flags) == -1) {
    LOG_ERROR(COMMON_POSIX_SOCKET_COMPONENT, "Failed to set socket flags: {}", strerror(errno));
  }
}

/**
 * @brief Creates a new listening socket.
 * @return A unique pointer to the created listening socket, or nullptr if the socket creation failed.
 */
std::unique_ptr<IListeningSocket> PosixSocket::create_listener() {
  auto sock = std::unique_ptr<PosixSocket>(new PosixSocket());
  if (!sock->is_valid()) {
    return nullptr;
  }
  return sock;
}

/**
 * @brief Creates a new connector socket to connect to a specified IP address and port.
 * @param ip_address The IP address to connect to.
 * @param port The port number to connect to.
 * @return A unique pointer to the created stream socket, or nullptr if the connection failed.
 */
std::unique_ptr<IStreamSocket> PosixSocket::create_connector(const std::string &ip_address, int port) {
  auto sock = std::unique_ptr<PosixSocket>(new PosixSocket());
  if (!sock->is_valid()) {
    return nullptr;
  }

  sockaddr_in server_addr{};
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);

  if (inet_pton(AF_INET, ip_address.c_str(), &server_addr.sin_addr) <= 0) {
    LOG_ERROR(COMMON_POSIX_SOCKET_COMPONENT, "Invalid IP address: {}", ip_address);
    return nullptr;
  }

  if (connect(sock->get_fd(), reinterpret_cast<sockaddr *>(&server_addr), sizeof(server_addr)) < 0) {
    LOG_ERROR(COMMON_POSIX_SOCKET_COMPONENT, "Connection failed: {}", strerror(errno));
    return nullptr;
  }

  return sock;
}

/**
 * @brief Binds the socket to a specified port.
 * @param port The port number to bind the socket to.
 * @return True if the binding was successful, false otherwise.
 */
bool PosixSocket::bind_socket(int port) {
  if (!is_valid())
    return false;

  int opt = 1;
  if (setsockopt(socket_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
    LOG_ERROR(COMMON_POSIX_SOCKET_COMPONENT, "Failed to set socket options: {}", strerror(errno));
    return false;
  }

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(port);

  if (bind(socket_fd_, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
    LOG_ERROR(COMMON_POSIX_SOCKET_COMPONENT, "Failed to bind socket: {}", strerror(errno));
    return false;
  }

  return true;
}

/**
 * @brief Listens for incoming connections on the socket.
 * @param backlog The maximum length of the queue of pending connections.
 * @return True if the socket is successfully set to listen, false otherwise.
 */
bool PosixSocket::listen_socket(int backlog) {
  if (!is_valid())
    return false;

  if (listen(socket_fd_, backlog) < 0) {
    LOG_ERROR(COMMON_POSIX_SOCKET_COMPONENT, "Failed to listen on socket: {}", strerror(errno));
    return false;
  }

  return true;
}

/**
 * @brief Accepts an incoming connection on the socket.
 * @return A unique pointer to a new stream socket representing the accepted connection, or nullptr if the accept failed.
 */
std::unique_ptr<IStreamSocket> PosixSocket::accept_connection() {
  if (!is_valid())
    return nullptr;

  int client_fd = accept(socket_fd_, nullptr, nullptr);
  if (client_fd < 0) {
    if (errno != EAGAIN && errno != EWOULDBLOCK) {
      LOG_ERROR(COMMON_POSIX_SOCKET_COMPONENT, "Failed to accept connection: {}", strerror(errno));
    }
    return nullptr;
  }

  return std::make_unique<PosixSocket>(client_fd);
}

/**
 * @brief Sends data over the socket.
 * @param data The data to send as a vector of characters.
 * @return A SocketResult indicating the status of the operation and the number of bytes sent.
 */
SocketResult PosixSocket::send_data(const std::vector<char> &data) {
  if (!is_valid())
    return {SocketStatus::ERROR, 0};

  ssize_t bytes_sent = send(socket_fd_, data.data(), data.size(), MSG_NOSIGNAL);
  if (bytes_sent < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return {SocketStatus::WOULD_BLOCK, 0};
    }
    // EPIPE means the other end closed the connection. Treat it as a closed socket.
    if (errno == ECONNRESET || errno == EPIPE) {
      return {SocketStatus::CLOSED, 0};
    }

    LOG_ERROR(COMMON_POSIX_SOCKET_COMPONENT, "Failed to send data: {}", strerror(errno));
    return {SocketStatus::ERROR, 0};
  }

  return {SocketStatus::OK, static_cast<std::size_t>(bytes_sent)};
}

/**
 * @brief Receives data from the socket into a buffer.
 * @param buffer The buffer to receive data into, which should be pre-allocated with sufficient capacity.
 */
SocketResult PosixSocket::receive_data(std::vector<char> &buffer) {
  if (!is_valid())
    return {SocketStatus::ERROR, 0};

  ssize_t bytes_received = ::recv(socket_fd_, buffer.data(), buffer.capacity(), 0);
  
  if (bytes_received < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return {SocketStatus::WOULD_BLOCK, 0};
    }

    LOG_ERROR(COMMON_POSIX_SOCKET_COMPONENT, "Failed to receive data: {}", strerror(errno));
    return {SocketStatus::ERROR, 0};
  }
  
  if (bytes_received == 0) {
    return {SocketStatus::CLOSED, 0};
  }

  return {SocketStatus::OK, static_cast<std::size_t>(bytes_received)};
}

} // namespace common
} // namespace chat_app