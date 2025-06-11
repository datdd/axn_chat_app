#ifndef COMMON_PROTOCOL_H
#define COMMON_PROTOCOL_H

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace chat_app {
namespace common {

constexpr uint32_t SERVER_ID = 0;
constexpr uint32_t BROADCAST_ID = 0;
constexpr uint32_t INVALID_ID = 0xFFFFFFFF;

/**
 * @brief Enum class representing the different types of messages
 * that can be sent in the chat application protocol.
 */
enum class MessageType : uint8_t {
  // --- Client to Server ---
  C2S_JOIN = 0x01,
  C2S_BROADCAST = 0x02,
  C2S_PRIVATE = 0x03,
  C2S_LEAVE = 0x04,
  C2S_USER_JOINED_LIST = 0x05,

  // --- Server to Client ---
  S2C_JOIN_SUCCESS = 0x10,
  S2C_JOIN_FAILURE = 0x11,
  S2C_BROADCAST = 0x12,
  S2C_PRIVATE = 0x13,
  S2C_USER_JOINED = 0x14,
  S2C_USER_LEFT = 0x15,
  S2C_USER_JOINED_LIST = 0x16,
  S2C_SERVER_SHUTDOWN = 0x17,

  S2C_ERROR = 0xFF
};

/**
 * @brief Represents the header of a message in the chat application protocol.
 */
struct MessageHeader {
  MessageType type;
  uint32_t sender_id;
  uint32_t receiver_id;
  uint32_t payload_size;

  MessageHeader() : type(MessageType::S2C_ERROR), sender_id(INVALID_ID), receiver_id(INVALID_ID), payload_size(0) {}

  MessageHeader(MessageType t, uint32_t sender, uint32_t receiver, uint32_t size)
      : type(t), sender_id(sender), receiver_id(receiver), payload_size(size) {}
};

// The fixed size of the header on the wire.
// 1 (type) + 4 (sender) + 4 (recipient) + 4 (size) = 13 bytes.
constexpr size_t HEADER_SIZE = sizeof(uint8_t) + sizeof(uint32_t) * 3;

/**
 * @brief Represents a high-level message in the chat application protocol.
 * Contains a header and a payload.
 */
struct Message {
  MessageHeader header;
  std::string payload;

  /**
   * @brief Default constructor for Message.
   * Initializes the header to an error type and sets payload to an empty string.
   */
  Message() : header(MessageType::S2C_ERROR, INVALID_ID, INVALID_ID, 0), payload("") {}

  /**
   * @brief Constructs a Message with the specified type, sender, receiver, and payload.
   * @param type The type of the message.
   * @param sender The ID of the sender.
   * @param receiver The ID of the receiver.
   * @param payload_size The size of the payload.
   * @param payload The payload data as a string.
   */
  Message(MessageType type, uint32_t sender, uint32_t receiver, uint32_t payload_size, const std::string &payload)
      : header(type, sender, receiver, payload_size), payload(payload) {}
  
  /**
   * @brief Constructs a Message with the specified type, sender, receiver, and payload.
   * This constructor automatically calculates the payload size from the string.
   * @param type The type of the message.
   * @param sender The ID of the sender.
   * @param receiver The ID of the receiver.
   * @param payload The payload data as a string.
   */
  Message(MessageType type, uint32_t sender, uint32_t receiver, const std::string &payload)
      : header(type, sender, receiver, static_cast<uint32_t>(payload.size())), payload(payload) {}
  
};

/**
 * @brief Serializes a high-level Message struct into a network-ready byte buffer.
 *
 * Handles endianness by converting multi-byte integers to network byte order
 * (using htonl for 32-bit integers).
 * @param msg The Message to serialize.
 * @return A vector of characters representing the serialized message.
 */
std::vector<char> serialize_message(const Message &msg);

/**
 * @brief Deserializes a byte buffer into a high-level Message struct.
 *
 * @param buffer The byte buffer containing the serialized message.
 * @return A pair containing the deserialized Message (if successful) and the
 *         number of bytes consumed from the buffer.
 */
std::pair<std::optional<Message>, size_t> deserialize_message(const std::vector<char> &buffer);

} // namespace common
} // namespace chat_app

#endif // COMMON_PROTOCOL_H