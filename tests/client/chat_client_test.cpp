#include "client/chat_client.h"
#include "client/server_connection.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using namespace chat_app::client;
using namespace chat_app::common;
using ::testing::_;
using ::testing::Return;
using ::testing::SaveArg;

class MockServerConnection : public ServerConnection {
public:
  MockServerConnection() {}
  MOCK_METHOD(bool, connect, (const std::string &, int), (override));
  MOCK_METHOD(void, disconnect, (), (override));
  MOCK_METHOD(void, send_message, (const Message &), (override));
  MOCK_METHOD(void, start_receiving, (const std::function<void(const Message &)> &), (override));
  MOCK_METHOD(bool, is_connected, (), (const, override));
};

class ChatClientTest : public ::testing::Test {
protected:
  void SetUp() override {
    auto server_connection_ptr = std::make_unique<MockServerConnection>();
    mock_server_connection = server_connection_ptr.get();
    client = std::make_unique<ChatClient>("test_user", std::move(server_connection_ptr));
  }

  MockServerConnection *mock_server_connection;
  std::unique_ptr<ChatClient> client;
  std::function<void(const Message &)> on_message_callback;
};

TEST_F(ChatClientTest, ConnectAndJoinSendsJoinRequestCorrectly) {
  // --- Arrange ---
  EXPECT_CALL(*mock_server_connection, connect("127.0.0.1", 12345)).WillOnce(Return(true));
  EXPECT_CALL(*mock_server_connection, start_receiving(_)).WillOnce(SaveArg<0>(&on_message_callback));

  Message sent_msg;
  EXPECT_CALL(*mock_server_connection, send_message(_)).WillOnce(SaveArg<0>(&sent_msg));

  // --- Act ---
  bool result = client->connect_and_join("127.0.0.1", 12345);

  // --- Assert ---
  ASSERT_TRUE(result);
  ASSERT_EQ(sent_msg.header.type, MessageType::C2S_JOIN);
  std::string username(sent_msg.payload.begin(), sent_msg.payload.end());
  ASSERT_EQ(username, "test_user");
}

TEST_F(ChatClientTest, OnJoinSuccessProcessesMessageCorrectly) {
  // --- Arrange ---
  EXPECT_CALL(*mock_server_connection, start_receiving(_)).WillOnce(SaveArg<0>(&on_message_callback));
  mock_server_connection->start_receiving([this](const Message &msg) {
    client->on_message_received(msg);
  });
  
  Message join_success_msg(MessageType::S2C_JOIN_SUCCESS, 0, 1, "test_user");

  // --- Act ---
  on_message_callback(join_success_msg);

  // --- Assert ---
  ASSERT_EQ(client->get_user_id(), 1);
  ASSERT_EQ(client->get_user_map().size(), 1);
  ASSERT_EQ(client->get_user_map().at(1), "test_user");
}

TEST_F(ChatClientTest, OnJoinFailureProcessesMessageCorrectly) {
  // --- Arrange ---
  EXPECT_CALL(*mock_server_connection, start_receiving(_)).WillOnce(SaveArg<0>(&on_message_callback));
  mock_server_connection->start_receiving([this](const Message &msg) {
    client->on_message_received(msg);
  });

  Message join_failure_msg(MessageType::S2C_JOIN_FAILURE, SERVER_ID, 0, "Username already taken");

  // --- Act ---
  on_message_callback(join_failure_msg);

  // --- Assert ---
  ASSERT_FALSE(client->is_running());
}

TEST_F(ChatClientTest, OnUserJoinedProcessesMessageCorrectly) {
  // --- Arrange ---
  EXPECT_CALL(*mock_server_connection, start_receiving(_)).WillOnce(SaveArg<0>(&on_message_callback));
  mock_server_connection->start_receiving([this](const Message &msg) {
    client->on_message_received(msg);
  });

  Message user_joined_msg(MessageType::S2C_USER_JOINED, 2, BROADCAST_ID, "new_user");

  // --- Act ---
  on_message_callback(user_joined_msg);

  // --- Assert ---
  ASSERT_EQ(client->get_user_map().size(), 1);
  ASSERT_EQ(client->get_user_map().at(2), "new_user");
}

TEST_F(ChatClientTest, OnUserLeftProcessesMessageCorrectly) {
  // --- Arrange ---
  EXPECT_CALL(*mock_server_connection, start_receiving(_)).WillOnce(SaveArg<0>(&on_message_callback));
  mock_server_connection->start_receiving([this](const Message &msg) {
    client->on_message_received(msg);
  });

  Message user_left_msg(MessageType::S2C_USER_LEFT, 2, BROADCAST_ID, "new_user");

  // --- Act ---
  on_message_callback(user_left_msg);

  // --- Assert ---
  ASSERT_EQ(client->get_user_map().size(), 0);
}

TEST_F(ChatClientTest, OnUserJoinedListProcessesMessageCorrectly) {
  // --- Arrange ---
  EXPECT_CALL(*mock_server_connection, start_receiving(_)).WillOnce(SaveArg<0>(&on_message_callback));
  mock_server_connection->start_receiving([this](const Message &msg) {
    client->on_message_received(msg);
  });

  Message user_list_msg(MessageType::S2C_USER_JOINED_LIST, SERVER_ID, BROADCAST_ID, "test_user:1,new_user:2");

  // --- Act ---
  on_message_callback(user_list_msg);

  // --- Assert ---
  ASSERT_EQ(client->get_user_map().size(), 2);
  ASSERT_EQ(client->get_user_map().at(1), "test_user");
  ASSERT_EQ(client->get_user_map().at(2), "new_user");
}