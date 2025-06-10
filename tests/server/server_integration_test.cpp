#include "common/protocol.h"
#include "common/socket.h"
#include "server/server.h"
#include "gtest/gtest.h"
#include <chrono>
#include <cstring>
#include <thread>

#include <arpa/inet.h>

using namespace chat_app::server;
using namespace chat_app::common;

class ServerIntegrationTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Start the server in a background thread
    server_thread_ = std::thread([this]() {
      Server server(port_);
      server_instance_ = &server;
      server.run();
      server_instance_ = nullptr;
    });

    // Give the server a moment to start up and listen
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  void TearDown() override {
    // Gracefully stop the server and join the thread
    if (server_instance_) {
      server_instance_->stop();
    }
    if (server_thread_.joinable()) {
      server_thread_.join();
    }
  }

  // Helper to read one message from a socket, with a timeout.
  std::optional<Message> read_message(IStreamSocket *socket) {
    std::vector<char> buffer;
    auto start_time = std::chrono::steady_clock::now();

    while (std::chrono::steady_clock::now() - start_time < std::chrono::seconds(2)) {
      std::vector<char> temp_buf(1024);
      auto result = socket->receive_data(temp_buf);

      if (result.status == SocketStatus::OK) {
        buffer.insert(buffer.end(), temp_buf.begin(), temp_buf.begin() + result.bytes_transferred);
        // Try to deserialize after receiving new data
        auto [msg_opt, bytes_consumed] = deserialize_message(buffer);
        if (msg_opt) {
          return msg_opt; // Success!
        }
      } else if (result.status == SocketStatus::WOULD_BLOCK) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        continue;
      } else {               // CLOSED or ERROR
        return std::nullopt; // Connection failed
      }
    }
    
    return std::nullopt; // Timeout
  }

  const int port_ = 9999;
  std::thread server_thread_;
  Server *server_instance_ = nullptr;
};

TEST_F(ServerIntegrationTest, ClientJoinsSuccessfully) {
  // 1. Connect to the server
  auto client_socket = PosixSocket::create_connector("127.0.0.1", port_);
  ASSERT_TRUE(client_socket && client_socket->is_valid());

  // 2. Send a JOIN request
  Message join_msg;
  join_msg.header.type = MessageType::C2S_JOIN;
  std::string username = "test_user";
  join_msg.payload.assign(username.begin(), username.end());
  join_msg.header.payload_size = join_msg.payload.size();

  auto serialized_join = serialize_message(join_msg);
  client_socket->send_data(serialized_join);

  // 3. Read and verify the response
  auto response = read_message(client_socket.get());
  ASSERT_TRUE(response.has_value());
  EXPECT_EQ(response->header.type, MessageType::S2C_JOIN_SUCCESS);
}

TEST_F(ServerIntegrationTest, BroadcastMessageMultipleClient) {
  // 1. Connect two clients to the server
  // Client 1
  auto client_socket1 = PosixSocket::create_connector("127.0.0.1", port_);
  ASSERT_TRUE(client_socket1 && client_socket1->is_valid());

  Message join_msg1(MessageType::C2S_JOIN, 0, 0, "user1");
  client_socket1->send_data(serialize_message(join_msg1));

  auto join_response1 = read_message(client_socket1.get());
  ASSERT_TRUE(join_response1.has_value());
  EXPECT_EQ(join_response1->header.type, MessageType::S2C_JOIN_SUCCESS);
  uint32_t client_id1 = join_response1->header.receiver_id;
  EXPECT_EQ(client_id1, 1);

  // Client 2
  auto client_socket2 = PosixSocket::create_connector("127.0.0.1", port_);
  ASSERT_TRUE(client_socket2 && client_socket2->is_valid());

  Message join_msg2(MessageType::C2S_JOIN, 0, 0, "user2");
  client_socket2->send_data(serialize_message(join_msg2));

  auto join_response2 = read_message(client_socket2.get());
  ASSERT_TRUE(join_response2.has_value());
  EXPECT_EQ(join_response2->header.type, MessageType::S2C_JOIN_SUCCESS);
  uint32_t client_id2 = join_response2->header.receiver_id;
  EXPECT_EQ(client_id2, 2);

  // 2. Client 1 sends a broadcast message
  Message broadcast_msg(MessageType::C2S_BROADCAST, client_id1, 0, "Hello from user1");
  client_socket1->send_data(serialize_message(broadcast_msg));

  // 3. Client 2 reads the broadcast message
  auto broadcast_response = read_message(client_socket2.get());
  ASSERT_TRUE(broadcast_response.has_value());
  EXPECT_EQ(broadcast_response->header.type, MessageType::S2C_BROADCAST);
  EXPECT_EQ(broadcast_response->header.sender_id, client_id1);
  EXPECT_EQ(broadcast_response->payload, "Hello from user1");
}

TEST_F(ServerIntegrationTest, RejectsClientWithTakenUsername) {
  // 1. Connect to the server
  auto client_socket1 = PosixSocket::create_connector("127.0.0.1", port_);
  ASSERT_TRUE(client_socket1 && client_socket1->is_valid());

  Message join_msg1(MessageType::C2S_JOIN, 0, 0, "test_user");
  client_socket1->send_data(serialize_message(join_msg1));

  auto response1 = read_message(client_socket1.get());
  ASSERT_TRUE(response1.has_value());
  EXPECT_EQ(response1->header.type, MessageType::S2C_JOIN_SUCCESS) << "Expected join success for first client";
  uint32_t client_id1 = response1->header.receiver_id;
  EXPECT_EQ(client_id1, 1) << "Expected client ID to be 1 for first client";

  // 2. Try to connect another client with the same username
  auto client_socket2 = PosixSocket::create_connector("127.0.0.1", port_);
  ASSERT_TRUE(client_socket2 && client_socket2->is_valid());

  Message join_msg2(MessageType::C2S_JOIN, 0, 0, "test_user");
  client_socket2->send_data(serialize_message(join_msg2));

  auto response2 = read_message(client_socket2.get());
  ASSERT_TRUE(response2.has_value());
  EXPECT_EQ(response2->header.type, MessageType::S2C_JOIN_FAILURE) << "Expected join failure for taken username";
  EXPECT_EQ(response2->header.receiver_id, INVALID_ID) << "Receiver ID should be INVALID_ID for join failure";
  EXPECT_EQ(response2->payload, "Username already taken") << "Expected error message for taken username";
}

TEST_F(ServerIntegrationTest, IgnoresInvalidMessages) {
  // 1. Connect to the server
  auto client_socket = PosixSocket::create_connector("127.0.0.1", port_);
  ASSERT_TRUE(client_socket && client_socket->is_valid());
  
  client_socket->set_non_blocking(true);
  
  // 2. Send an invalid message
  std::vector<char> invalid_data = {'I', 'n', 'v', 'a', 'l', 'i', 'd'};
  client_socket->send_data(invalid_data);
  
  // 3. Attempt to read a message
  auto response = read_message(client_socket.get());
  ASSERT_FALSE(response.has_value()); 
}

// Add this test to tests/server/server_integration_test.cc

TEST_F(ServerIntegrationTest, IgnoresMessagesFromUnauthenticatedClient) {
  // 1. A client connects but does NOT send a JOIN message.
  auto client_socket = PosixSocket::create_connector("127.0.0.1", port_);
  ASSERT_TRUE(client_socket && client_socket->is_valid());

  client_socket->set_non_blocking(true);

  // 2. The client immediately tries to send a broadcast message.
  Message broadcast_req(MessageType::C2S_BROADCAST, 123, BROADCAST_ID, "This is a test message");
  client_socket->send_data(serialize_message(broadcast_req));

  // 3. Verify that the server does NOT send any response.
  // A read should time out, proving the server ignored the invalid message.
  auto response = read_message(client_socket.get());
  EXPECT_FALSE(response.has_value());
}
