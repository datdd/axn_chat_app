#ifndef COMMON_SOCKET_H
#define COMMON_SOCKET_H

#include <cstddef>
#include <memory>
#include <string>
#include <vector>
#include <ostream>

namespace chat_app {
namespace common {

#define COMMON_POSIX_SOCKET_COMPONENT "PosixSocket"

enum class SocketStatus { OK, WOULD_BLOCK, CLOSED, ERROR };

/**
 * @brief Represents the result of a socket operation.
 * Contains the status of the operation and the number of bytes transferred.
 */
struct SocketResult {
  SocketStatus status;
  std::size_t bytes_transferred;
};

/**
 * @brief Overloaded operator<< for SocketStatus.
 */
inline std::ostream &operator<<(std::ostream &os, const SocketStatus &status) {
  switch (status) {
  case SocketStatus::OK:
    os << "OK";
    break;
  case SocketStatus::WOULD_BLOCK:
    os << "WOULD_BLOCK";
    break;
  case SocketStatus::CLOSED:
    os << "CLOSED";
    break;
  case SocketStatus::ERROR:
    os << "ERROR";
    break;
  default:
    os << "UNKNOWN";
    break;
  }
  return os;
}

class IStreamSocket; // Forward declaration

/**
 * @brief Interface for a listening socket that can accept incoming connections.
 */
class IListeningSocket {
public:
  virtual ~IListeningSocket() = default;

  virtual bool bind_socket(int port) = 0;
  virtual bool listen_socket(int backlog) = 0;
  virtual std::unique_ptr<IStreamSocket> accept_connection() = 0;
  virtual void close_socket() = 0;
  virtual bool is_valid() const = 0;
  virtual int get_fd() const = 0;
  virtual void set_non_blocking(bool non_blocking) = 0;
};

/**
 * @brief Interface for a stream socket that can send and receive data.
 */
class IStreamSocket {
public:
  virtual ~IStreamSocket() = default;

  virtual SocketResult send_data(const std::vector<char> &data) = 0;
  virtual SocketResult receive_data(std::vector<char> &buffer) = 0;
  virtual SocketResult raw_receive(char *buffer, size_t len) = 0;
  virtual void close_socket() = 0;
  virtual bool is_valid() const = 0;
  virtual int get_fd() const = 0;
  virtual void set_non_blocking(bool non_blocking) = 0;
};

/**
 * @brief Represents a POSIX socket that implements both IStreamSocket and IListeningSocket interfaces.
 */
class PosixSocket : public IStreamSocket, public IListeningSocket {
public:
  static std::unique_ptr<IListeningSocket> create_listener();
  static std::unique_ptr<IStreamSocket> create_connector(const std::string &ip_address, int port);

  explicit PosixSocket(int fd);
  ~PosixSocket() override;

  PosixSocket(const PosixSocket &) = delete;
  PosixSocket &operator=(const PosixSocket &) = delete;

  // IStreamSocket methods
  SocketResult send_data(const std::vector<char> &data) override;
  SocketResult receive_data(std::vector<char> &buffer) override;
  SocketResult raw_receive(char *buffer, size_t len) override;

  // IListeningSocket methods
  bool bind_socket(int port) override;
  bool listen_socket(int backlog) override;
  std::unique_ptr<IStreamSocket> accept_connection() override;

  // Common methods
  void close_socket() override;
  bool is_valid() const override;
  int get_fd() const override;
  void set_non_blocking(bool non_blocking) override;

private:
  PosixSocket();
  int socket_fd_{-1};
};

} // namespace common
} // namespace chat_app

#endif // COMMON_SOCKET_H