#ifndef SERVER_EPOLL_MANAGER_H
#define SERVER_EPOLL_MANAGER_H

#include <sys/epoll.h>
#include <vector>

namespace chat_app {
namespace server {

#define EPOLL_MANAGER_COMPONENT "EpollManager"

/**
 * @brief Manages epoll events.
 */
class EpollManager {
public:
  EpollManager(int max_events = 10);
  ~EpollManager();

  bool add_fd(int fd, uint32_t events);
  bool modify_fd(int fd, uint32_t events);
  bool remove_fd(int fd);

  int wait(int timeout);
  const epoll_event *get_events() const { return events_.data(); }

private:
  int epoll_fd_{-1};
  std::vector<epoll_event> events_;
};

} // namespace server
} // namespace chat_app

#endif // SERVER_EPOLL_MANAGER_H