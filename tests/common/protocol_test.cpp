#include "common/protocol.h"
#include "gtest/gtest.h"
#include <arpa/inet.h>
#include <cstring>

using namespace chat_app::common;

TEST(ProtocolTest, SerializeMessageWithPayload) {
  Message msg;
  msg.header.type = MessageType::C2S_JOIN;
  msg.header.sender_id = 12345;
  msg.header.receiver_id = SERVER_ID;
  msg.payload = "User1";
  msg.header.payload_size = msg.payload.length();

  std::vector<char> buffer = serialize_message(msg);

  const size_t expected_size = HEADER_SIZE + 5;
  ASSERT_EQ(buffer.size(), expected_size);

  EXPECT_EQ(static_cast<MessageType>(buffer[0]), MessageType::C2S_JOIN);

  uint32_t sender_id, receiver_id, payload_size;
  std::memcpy(&sender_id, buffer.data() + 1, sizeof(uint32_t));
  std::memcpy(&receiver_id, buffer.data() + 5, sizeof(uint32_t));
  std::memcpy(&payload_size, buffer.data() + 9, sizeof(uint32_t));

  EXPECT_EQ(ntohl(sender_id), 12345);
  EXPECT_EQ(ntohl(receiver_id), SERVER_ID);
  EXPECT_EQ(ntohl(payload_size), 5);

  std::string payload_content(buffer.data() + HEADER_SIZE, 5);
  EXPECT_EQ(payload_content, "User1");
}

TEST(ProtocolTest, SerializeMessageWithoutPayload) {
  Message msg;
  msg.header.type = MessageType::C2S_LEAVE;
  msg.header.sender_id = 67890;
  msg.header.receiver_id = SERVER_ID;
  msg.payload = "";
  msg.header.payload_size = 0;

  std::vector<char> buffer = serialize_message(msg);

  const size_t expected_size = HEADER_SIZE;
  ASSERT_EQ(buffer.size(), expected_size);

  EXPECT_EQ(static_cast<MessageType>(buffer[0]), MessageType::C2S_LEAVE);

  uint32_t sender_id, receiver_id, payload_size;
  std::memcpy(&sender_id, buffer.data() + 1, sizeof(uint32_t));
  std::memcpy(&receiver_id, buffer.data() + 5, sizeof(uint32_t));
  std::memcpy(&payload_size, buffer.data() + 9, sizeof(uint32_t));

  EXPECT_EQ(ntohl(sender_id), 67890);
  EXPECT_EQ(ntohl(receiver_id), SERVER_ID);
  EXPECT_EQ(ntohl(payload_size), 0);
}

TEST(ProtocolTest, DeserializeMessageWithPayload) {
  Message msg;
  msg.header.type = MessageType::S2C_BROADCAST;
  msg.header.sender_id = 12345;
  msg.header.receiver_id = SERVER_ID;
  msg.payload = "Hello";
  msg.header.payload_size = msg.payload.length();

  std::vector<char> buffer = serialize_message(msg);
  
  auto [deserialized_msg, size] = deserialize_message(buffer);
  
  ASSERT_TRUE(deserialized_msg.has_value());
  EXPECT_EQ(deserialized_msg->header.type, MessageType::S2C_BROADCAST);
  EXPECT_EQ(deserialized_msg->header.sender_id, 12345);
  EXPECT_EQ(deserialized_msg->header.receiver_id, SERVER_ID);
  EXPECT_EQ(deserialized_msg->header.payload_size, 5);
  EXPECT_EQ(deserialized_msg->payload, "Hello");
}

TEST(ProtocolTest, DeserializeMessageWithoutPayload) {
  Message msg;
  msg.header.type = MessageType::C2S_LEAVE;
  msg.header.sender_id = 67890;
  msg.header.receiver_id = SERVER_ID;
  msg.payload = "";
  msg.header.payload_size = 0;

  std::vector<char> buffer = serialize_message(msg);
  
  auto [deserialized_msg, size] = deserialize_message(buffer);
  
  ASSERT_TRUE(deserialized_msg.has_value());
  EXPECT_EQ(deserialized_msg->header.type, MessageType::C2S_LEAVE);
  EXPECT_EQ(deserialized_msg->header.sender_id, 67890);
  EXPECT_EQ(deserialized_msg->header.receiver_id, SERVER_ID);
  EXPECT_EQ(deserialized_msg->header.payload_size, 0);
  EXPECT_EQ(deserialized_msg->payload, "");
}

TEST(ProtocolTest, DeserializeIncompleteMessage) {
  Message msg;
  msg.header.type = MessageType::C2S_JOIN;
  msg.header.sender_id = 12345;
  msg.header.receiver_id = SERVER_ID;
  msg.payload = "User1";
  msg.header.payload_size = msg.payload.length();

  std::vector<char> buffer = serialize_message(msg);
  
  // Simulate an incomplete message by truncating the buffer
  buffer.resize(buffer.size() - 1);
  
  auto [deserialized_msg, size] = deserialize_message(buffer);
  
  EXPECT_FALSE(deserialized_msg.has_value());
  EXPECT_EQ(size, 0);
}

TEST(ProtocolTest, DeserializeBufferWithExtraData) {
  Message msg;
  msg.header.type = MessageType::S2C_BROADCAST;
  msg.header.sender_id = 12345;
  msg.header.receiver_id = SERVER_ID;
  msg.payload = "Hello";
  msg.header.payload_size = msg.payload.length();

  std::vector<char> buffer = serialize_message(msg);
  
  // Add extra data to the buffer
  buffer.push_back('X');
  
  auto [deserialized_msg, size] = deserialize_message(buffer);
  
  ASSERT_TRUE(deserialized_msg.has_value());
  EXPECT_EQ(deserialized_msg->header.type, MessageType::S2C_BROADCAST);
  EXPECT_EQ(deserialized_msg->header.sender_id, 12345);
  EXPECT_EQ(deserialized_msg->header.receiver_id, SERVER_ID);
  EXPECT_EQ(deserialized_msg->header.payload_size, 5);
  EXPECT_EQ(deserialized_msg->payload, "Hello");
}

TEST(ProtocolTest, SerializeAndDeserializeMultipleMessages) {
  std::vector<Message> messages = {
      {MessageHeader{MessageType::C2S_JOIN, 12345, SERVER_ID, 5}, "User1"},
      {MessageHeader{MessageType::C2S_LEAVE, 67890, SERVER_ID, 0}, ""},
      {MessageHeader{MessageType::S2C_BROADCAST, 54321, SERVER_ID, 5}, "Hello"}
  };
  std::vector<char> buffer;
  for (const auto& msg : messages) {
    auto serialized = serialize_message(msg);
    buffer.insert(buffer.end(), serialized.begin(), serialized.end());
  }
  size_t offset = 0;
  for (const auto& expected_msg : messages) {
    auto [deserialized_msg, size] = deserialize_message(
        std::vector<char>(buffer.begin() + offset, buffer.end()));
    ASSERT_TRUE(deserialized_msg.has_value());
    EXPECT_EQ(deserialized_msg->header.type, expected_msg.header.type);
    EXPECT_EQ(deserialized_msg->header.sender_id, expected_msg.header.sender_id);
    EXPECT_EQ(deserialized_msg->header.receiver_id, expected_msg.header.receiver_id);
    EXPECT_EQ(deserialized_msg->header.payload_size, expected_msg.header.payload_size);
    EXPECT_EQ(deserialized_msg->payload, expected_msg.payload);
    offset += HEADER_SIZE + expected_msg.header.payload_size;
  }
  EXPECT_EQ(offset, buffer.size());
}