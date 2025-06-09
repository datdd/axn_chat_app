#include "common/socket.h"
#include <gtest/gtest.h>

#include <arpa/inet.h>
#include <netinet/in.h>

using namespace chat_app::common;

/**
 * @brief Helper function to get the port number from a socket file descriptor.
 *
 * @param sockfd The socket file descriptor.
 * @return The port number on success, or -1 on error.
 */
int get_socket_port(int sockfd) {
  struct sockaddr_in addr;
  socklen_t addr_len = sizeof(addr);
  if (getsockname(sockfd, (struct sockaddr *)&addr, &addr_len) == -1) {
    return -1;
  }
  return ntohs(addr.sin_port);
}

class SocketTest : public ::testing::Test {
protected:
  void SetUp() override {
    listener_ = PosixSocket::create_listener();
    ASSERT_TRUE(listener_ != nullptr) << "Failed to create listener socket";
    ASSERT_TRUE(listener_->is_valid()) << "Listener socket is not valid";
    ASSERT_TRUE(listener_->bind_socket(0)) << "Failed to bind listener socket";

    listening_port_ = get_socket_port(listener_->get_fd());
    ASSERT_NE(listening_port_, -1) << "Failed to get listening port";

    ASSERT_TRUE(listener_->listen_socket(5)) << "Failed to listen on socket";
  }

  void TearDown() override {
    if (listener_) {
      listener_->close_socket();
    }
  }

  std::unique_ptr<IListeningSocket> listener_;
  int listening_port_ = -1;
};

TEST_F(SocketTest, ConnectionSucess) {
  std::unique_ptr<IStreamSocket> client_socket = PosixSocket::create_connector("127.0.0.1", listening_port_);

  ASSERT_TRUE(client_socket != nullptr) << "Failed to create client socket";
  EXPECT_TRUE(client_socket->is_valid()) << "Client socket is not valid";

  // Accept the connection on the listener socket
  std::unique_ptr<IStreamSocket> accepted_socket = listener_->accept_connection();
  ASSERT_TRUE(accepted_socket != nullptr) << "Failed to accept connection";
  EXPECT_TRUE(accepted_socket->is_valid()) << "Accepted socket is not valid";
}

TEST_F(SocketTest, ConnectionFailure) {
  // Attempt to connect to an invalid port
  std::unique_ptr<IStreamSocket> client_socket = PosixSocket::create_connector("127.0.0.1", listening_port_ + 1);
  EXPECT_TRUE(client_socket == nullptr) << "Connection should have failed but succeeded";
}

TEST_F(SocketTest, SendAndReceiveData) {
  std::unique_ptr<IStreamSocket> client_socket = PosixSocket::create_connector("127.0.0.1", listening_port_);
  ASSERT_TRUE(client_socket != nullptr) << "Failed to create client socket";
  ASSERT_TRUE(client_socket->is_valid()) << "Client socket is not valid";

  std::unique_ptr<IStreamSocket> accepted_socket = listener_->accept_connection();
  ASSERT_TRUE(accepted_socket != nullptr) << "Failed to accept connection";
  ASSERT_TRUE(accepted_socket->is_valid()) << "Accepted socket is not valid";

  // Client sends, Server receives
  {
    std::vector<char> send_buffer = {'H', 'e', 'l', 'l', 'o'};
    std::vector<char> receive_buffer(send_buffer.size());
    
    SocketResult send_result = client_socket->send_data(send_buffer);
    ASSERT_EQ(send_result.status, SocketStatus::OK) << "Failed to send data";
    ASSERT_EQ(send_result.bytes_transferred, send_buffer.size()) << "Sent data size mismatch";

    SocketResult receive_result = accepted_socket->receive_data(receive_buffer);
    ASSERT_EQ(receive_result.status, SocketStatus::OK) << "Failed to receive data";
    ASSERT_EQ(receive_result.bytes_transferred, send_buffer.size()) << "Received data size mismatch";

    EXPECT_EQ(send_buffer, receive_buffer);
  }

  // Server sends, Client receives
  {
    std::vector<char> send_buffer = {'W', 'o', 'r', 'l', 'd'};
    std::vector<char> receive_buffer(send_buffer.size());

    SocketResult send_result = accepted_socket->send_data(send_buffer);
    ASSERT_EQ(send_result.status, SocketStatus::OK) << "Failed to send data";
    ASSERT_EQ(send_result.bytes_transferred, send_buffer.size()) << "Sent data size mismatch";

    SocketResult receive_result = client_socket->receive_data(receive_buffer);
    ASSERT_EQ(receive_result.status, SocketStatus::OK) << "Failed to receive data";
    ASSERT_EQ(receive_result.bytes_transferred, send_buffer.size()) << "Received data size mismatch";
    EXPECT_EQ(send_buffer, receive_buffer);
  }
}

TEST_F(SocketTest, NonBlockingSocket) {
  std::unique_ptr<IStreamSocket> client_socket = PosixSocket::create_connector("127.0.0.1", listening_port_);
  ASSERT_TRUE(client_socket != nullptr) << "Failed to create client socket";
  ASSERT_TRUE(client_socket->is_valid()) << "Client socket is not valid";
  client_socket->set_non_blocking(true);
  
  std::unique_ptr<IStreamSocket> accepted_socket = listener_->accept_connection();
  ASSERT_TRUE(accepted_socket != nullptr) << "Failed to accept connection";
  ASSERT_TRUE(accepted_socket->is_valid()) << "Accepted socket is not valid";
  accepted_socket->set_non_blocking(true);
  
  std::vector<char> send_buffer = {'N', 'o', 'n', '-', 'B', 'l', 'o', 'c', 'k', 'i', 'n', 'g'};
  SocketResult send_result = client_socket->send_data(send_buffer);
  ASSERT_EQ(send_result.status, SocketStatus::OK) << "Failed to send data";
  ASSERT_EQ(send_result.bytes_transferred, send_buffer.size()) << "Sent data size mismatch";
  
  std::vector<char> receive_buffer(send_buffer.size());
  SocketResult receive_result = accepted_socket->receive_data(receive_buffer);
  ASSERT_EQ(receive_result.status, SocketStatus::OK) << "Failed to receive data";
  ASSERT_EQ(receive_result.bytes_transferred, send_buffer.size()) << "Received data size mismatch";
  
  EXPECT_EQ(send_buffer, receive_buffer);
  // Check that the non-blocking behavior works
  receive_result = accepted_socket->receive_data(receive_buffer);
  EXPECT_EQ(receive_result.status, SocketStatus::WOULD_BLOCK) << "Expected non-blocking receive to return WOULD_BLOCK";
  receive_result = client_socket->receive_data(receive_buffer);
  EXPECT_EQ(receive_result.status, SocketStatus::WOULD_BLOCK) << "Expected non-blocking receive to return WOULD_BLOCK";
}

TEST_F(SocketTest, DetectsClosedConnection) {
  std::unique_ptr<IStreamSocket> client_socket = PosixSocket::create_connector("127.0.0.1", listening_port_);
  ASSERT_TRUE(client_socket != nullptr) << "Failed to create client socket";
  ASSERT_TRUE(client_socket->is_valid()) << "Client socket is not valid";
  std::unique_ptr<IStreamSocket> accepted_socket = listener_->accept_connection();
  ASSERT_TRUE(accepted_socket != nullptr) << "Failed to accept connection";
  ASSERT_TRUE(accepted_socket->is_valid()) << "Accepted socket is not valid";
  
  // Close the accepted socket
  client_socket->close_socket();

  std::vector<char> receive_buffer(10);
  SocketResult receive_result = accepted_socket->receive_data(receive_buffer);
  EXPECT_EQ(receive_result.status, SocketStatus::CLOSED) << "Expected receive on closed socket to return CLOSED";
  EXPECT_EQ(receive_result.bytes_transferred, 0) << "Expected no bytes to be transferred on closed socket";
}