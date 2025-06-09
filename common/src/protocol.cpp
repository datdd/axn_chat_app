#include "common/protocol.h"
#include <arpa/inet.h> 
#include <cstring> 

namespace chat_app {
namespace common {

std::vector<char> serialize_message(const Message &msg) {
  // Total size is the fixed header size plus the dynamic payload size.
  const size_t total_size = HEADER_SIZE + msg.header.payload_size;
  std::vector<char> buffer(total_size);

  char *ptr = buffer.data();

  // 1. Serialize Type
  *reinterpret_cast<uint8_t *>(ptr) = static_cast<uint8_t>(msg.header.type);
  ptr += sizeof(uint8_t);

  // 2. Serialize Sender ID (in network byte order)
  uint32_t sender_id_net = htonl(msg.header.sender_id);
  std::memcpy(ptr, &sender_id_net, sizeof(uint32_t));
  ptr += sizeof(uint32_t);

  // 3. Serialize Recipient ID (in network byte order)
  uint32_t receiver_id_net = htonl(msg.header.receiver_id);
  std::memcpy(ptr, &receiver_id_net, sizeof(uint32_t));
  ptr += sizeof(uint32_t);

  // 4. Serialize Payload Size (in network byte order)
  uint32_t payload_size_net = htonl(msg.header.payload_size);
  std::memcpy(ptr, &payload_size_net, sizeof(uint32_t));
  ptr += sizeof(uint32_t);

  // 5. Serialize Payload
  if (msg.header.payload_size > 0) {
    std::memcpy(ptr, msg.payload.data(), msg.header.payload_size);
  }

  return buffer;
}

std::pair<std::optional<Message>, size_t> deserialize_message(const std::vector<char> &buffer) {
  if (buffer.size() < HEADER_SIZE) {
    return {std::nullopt, 0}; // Not enough data for a header
  }

  const char *ptr = buffer.data();

  // 1. Peek at the payload size from the header to see if the full message is present.
  uint32_t payload_size_net;
  std::memcpy(&payload_size_net, ptr + sizeof(uint8_t) + sizeof(uint32_t) * 2, sizeof(uint32_t));
  uint32_t payload_size = ntohl(payload_size_net);

  const size_t total_message_size = HEADER_SIZE + payload_size;
  if (buffer.size() < total_message_size) {
    return {std::nullopt, 0}; // Incomplete message
  }

  // Create a Message object to hold the deserialized data
  Message msg;

  // 2. Deserialize Type
  msg.header.type = static_cast<MessageType>(*reinterpret_cast<const uint8_t *>(ptr));
  ptr += sizeof(uint8_t);

  // 3. Deserialize Sender ID
  uint32_t sender_id_net;
  std::memcpy(&sender_id_net, ptr, sizeof(uint32_t));
  msg.header.sender_id = ntohl(sender_id_net);
  ptr += sizeof(uint32_t);

  // 4. Deserialize Recipient ID
  uint32_t receiver_id_net;
  std::memcpy(&receiver_id_net, ptr, sizeof(uint32_t));
  msg.header.receiver_id = ntohl(receiver_id_net);
  ptr += sizeof(uint32_t);

  // 5. The payload size is already deserialized
  msg.header.payload_size = payload_size;
  ptr += sizeof(uint32_t);

  // 6. Deserialize Payload
  if (msg.header.payload_size > 0) {
    msg.payload.assign(ptr, msg.header.payload_size);
  }

  return {msg, total_message_size};
}

} // namespace common
} // namespace chat_app