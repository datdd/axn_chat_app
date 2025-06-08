#include "server/client_session.h"

namespace chat_app {
namespace server {
  
ClientSession::ClientSession(uint32_t id, std::unique_ptr<common::IStreamSocket> socket)
    : id_(id), socket_(std::move(socket)) {}

} // namespace server
} // namespace chat_app