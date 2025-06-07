#include "common/protocol.h"
#include <arpa/inet.h>
#include <cstring>

namespace chat_app {
namespace common {

std::vector<char> serialize_message(const Message &msg) {
  std::vector<char> buffer(HEADER_SIZE + msg.payload.size());
  auto *header = reinterpret_cast<MessageHeader *>(buffer.data());

  header->type = msg.header.type;
  header->sender_id = htonl(msg.header.sender_id);
  header->receiver_id = htonl(msg.header.receiver_id);
  header->payload_size = htonl(static_cast<uint32_t>(msg.payload.size()));

  std::memcpy(buffer.data() + HEADER_SIZE, msg.payload.data(), msg.payload.size());

  return buffer;
}

std::pair<std::optional<Message>, size_t> deserialize_message(const std::vector<char> &buffer) {
  if (buffer.size() < HEADER_SIZE) {
    return {std::nullopt, 0};
  }

  const auto *header = reinterpret_cast<const MessageHeader *>(buffer.data());

  Message msg;
  msg.header.type = header->type;
  msg.header.sender_id = ntohl(header->sender_id);
  msg.header.receiver_id = ntohl(header->receiver_id);
  msg.header.payload_size = ntohl(header->payload_size);

  if (msg.header.payload_size + HEADER_SIZE > buffer.size()) {
    return {std::nullopt, 0};
  }

  msg.payload.resize(msg.header.payload_size);
  std::memcpy(msg.payload.data(), buffer.data() + HEADER_SIZE, msg.header.payload_size);

  return {msg, HEADER_SIZE + msg.header.payload_size};
}

} // namespace common
} // namespace chat_app
