#include "server/client_session.h"

namespace chat_app {
namespace server {

/**
 * @brief Constructs a ClientSession with a unique ID and a socket.
 * @param id The unique ID for the client session.
 * @param socket A unique pointer to the socket used for communication.
 */
ClientSession::ClientSession(uint32_t id, std::unique_ptr<common::IStreamSocket> socket)
    : id_(id), socket_(std::move(socket)) {}

} // namespace server
} // namespace chat_app