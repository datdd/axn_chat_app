#include "server/client_manager.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <memory>

using namespace chat_app::server;
using namespace chat_app::common;
using ::testing::_;
using ::testing::Return;

class MockStreamSocket : public IStreamSocket {
public:
  virtual ~MockStreamSocket() = default;
  MOCK_METHOD(SocketResult, send_data, (const std::vector<char> &), (override));
  MOCK_METHOD(SocketResult, receive_data, (std::vector<char> &), (override));
  MOCK_METHOD(SocketResult, raw_receive, (char *, size_t), (override));
  MOCK_METHOD(void, close_socket, (), (override));
  MOCK_METHOD(bool, is_valid, (), (const, override));
  MOCK_METHOD(int, get_fd, (), (const, override));
  MOCK_METHOD(void, set_non_blocking, (bool), (override));
};

class ClientManagerTest : public ::testing::Test {
protected:
  void SetUp() override { client_manager_ = std::make_unique<ClientManager>(); }

  void TearDown() override { client_manager_.reset(); }

  std::unique_ptr<ClientManager> client_manager_;
};

TEST_F(ClientManagerTest, AddClientAndGetClient) {
  auto mock_socket = std::make_unique<MockStreamSocket>();
  EXPECT_CALL(*mock_socket, get_fd()).WillRepeatedly(Return(5));

  ClientSession *session = client_manager_->add_client(std::move(mock_socket));
  ASSERT_NE(session, nullptr) << "Failed to add client session";
  EXPECT_EQ(session->get_fd(), 5) << "Client session FD does not match";
  EXPECT_EQ(client_manager_->get_client_by_fd(5), session) << "Failed to retrieve client by FD";
}

TEST_F(ClientManagerTest, RemoveClient) {
  auto mock_socket = std::make_unique<MockStreamSocket>();
  EXPECT_CALL(*mock_socket, get_fd()).WillRepeatedly(Return(10));

  ClientSession *session = client_manager_->add_client(std::move(mock_socket));
  ASSERT_NE(session, nullptr) << "Failed to add client session";

  client_manager_->remove_client(session->get_fd());
  EXPECT_EQ(client_manager_->get_client_by_fd(10), nullptr) << "Client session was not removed";
  EXPECT_EQ(client_manager_->get_client_by_id(session->get_id()), nullptr)
      << "Client session ID still exists after removal";
}

TEST_F(ClientManagerTest, GetClientById) {
  auto mock_socket = std::make_unique<MockStreamSocket>();
  EXPECT_CALL(*mock_socket, get_fd()).WillRepeatedly(Return(15));

  ClientSession *session = client_manager_->add_client(std::move(mock_socket));
  ASSERT_NE(session, nullptr) << "Failed to add client session";

  EXPECT_EQ(client_manager_->get_client_by_id(session->get_id()), session) << "Failed to retrieve client by ID";
}

TEST_F(ClientManagerTest, GetAllClients) {
  auto mock_socket1 = std::make_unique<MockStreamSocket>();
  EXPECT_CALL(*mock_socket1, get_fd()).WillRepeatedly(Return(20));
  client_manager_->add_client(std::move(mock_socket1));

  auto mock_socket2 = std::make_unique<MockStreamSocket>();
  EXPECT_CALL(*mock_socket2, get_fd()).WillRepeatedly(Return(25));
  client_manager_->add_client(std::move(mock_socket2));

  auto clients = client_manager_->get_all_clients();
  ASSERT_EQ(clients.size(), 2) << "Expected two clients in the manager";
}

TEST_F(ClientManagerTest, UsernameTaken) {
  auto mock_socket = std::make_unique<MockStreamSocket>();
  EXPECT_CALL(*mock_socket, get_fd()).WillRepeatedly(Return(30));

  ClientSession *session = client_manager_->add_client(std::move(mock_socket));
  ASSERT_NE(session, nullptr) << "Failed to add client session";

  session->set_username("test_user");
  client_manager_->add_username("test_user");
  EXPECT_TRUE(client_manager_->is_username_taken("test_user")) << "Username should be marked as taken";
  EXPECT_FALSE(client_manager_->is_username_taken("another_user")) << "Another username should not be taken";
}

TEST_F(ClientManagerTest, BroadcastMessage) {
  auto mock_socket1 = std::make_unique<MockStreamSocket>();
  auto mock_socket2 = std::make_unique<MockStreamSocket>();

  MockStreamSocket *raw_socket1 = mock_socket1.get();
  MockStreamSocket *raw_socket2 = mock_socket2.get();

  EXPECT_CALL(*raw_socket1, get_fd()).WillRepeatedly(Return(35));
  EXPECT_CALL(*raw_socket2, get_fd()).WillRepeatedly(Return(40));

  EXPECT_CALL(*raw_socket1, send_data(_)).Times(0);
  EXPECT_CALL(*raw_socket2, send_data(_)).Times(1);

  ClientSession *session1 = client_manager_->add_client(std::move(mock_socket1));
  session1->set_authenticated(true);

  ClientSession *session2 = client_manager_->add_client(std::move(mock_socket2));
  session2->set_authenticated(true);

  Message msg(MessageType::S2C_BROADCAST, session1->get_id(), BROADCAST_ID, "hi");
  client_manager_->broadcast_message(msg, session1->get_id());
}