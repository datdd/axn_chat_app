#ifndef COMMON_PROTOCOL_H
#define COMMON_PROTOCOL_H

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace chat_app {
namespace common {

constexpr uint32_t SERVER_ID = 0;
constexpr uint32_t BROADCAST_ID = 0;

enum class MessageType : uint8_t {
  // Message sent to the server
  C2S_JOIN = 0x01,
  C2S_BROADCAST = 0x02,
  C2S_PRIVATE = 0x03,
  C2S_LEAVE = 0x04,

  // Message sent from the server
  S2C_JOIN_SUCCESS = 0x81,
  S2C_JOIN_FAILURE = 0x82,
  S2C_BROADCAST = 0x83,
  S2C_PRIVATE = 0x84,
  S2C_USER_JOINED = 0x85,
  S2C_USER_LEFT = 0x86,
  S2C_ERROR = 0x87,
};

struct MessageHeader {
  MessageType type;      // Type of the message
  uint32_t sender_id;    // ID of the sender
  uint32_t receiver_id;  // ID of the receiver (0 for broadcast)
  uint32_t payload_size; // Length of the message in bytes
};

constexpr std::size_t HEADER_SIZE = sizeof(uint8_t) + sizeof(uint32_t) * 3; // MessageType + length + sender_id + receiver_id

struct Message {
  MessageHeader header;
  std::vector<char> payload;
};

/**
 * @brief Serializes a Message object into a byte vector.
 *
 * @param msg The Message object to serialize.
 * @return A vector of bytes representing the serialized message.
 */
std::vector<char> serialize_message(const Message &msg);

/**
 * @brief Deserializes a byte vector into a Message object.
 *
 * @param data The byte vector to deserialize.
 * @return An optional Message object if deserialization is successful, otherwise std::nullopt.
 */
std::pair<std::optional<Message>, size_t> deserialize_message(const std::vector<char> &buffer);

} // namespace common
} // namespace chat_app

#endif // COMMON_PROTOCOL_H