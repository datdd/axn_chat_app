#include "server/epoll_manager.h"
#include "common/logger.h"
#include <cerrno>
#include <cstring>
#include <unistd.h>

namespace chat_app {
namespace server {

/**
 * @brief Constructs an EpollManager with a specified maximum number of events.
 */
EpollManager::EpollManager(int max_events) : events_(max_events) {
  epoll_fd_ = epoll_create1(0);
  if (epoll_fd_ == -1) {
    LOG_ERROR(EPOLL_MANAGER_COMPONENT, "Failed to create epoll instance: {}", std::strerror(errno));
  }
}

/**
 * @brief Destructor for EpollManager.
 * Closes the epoll file descriptor if it is valid.
 */
EpollManager::~EpollManager() {
  if (epoll_fd_ != -1) {
    close(epoll_fd_);
  }
}

/**
 * @brief Adds a file descriptor to the epoll instance with specified events.
 * @param fd The file descriptor to add.
 * @param events The events to monitor for the file descriptor.
 * @return True if the operation was successful, false otherwise.
 */
bool EpollManager::add_fd(int fd, uint32_t events) {
  epoll_event event;
  event.events = events;
  event.data.fd = fd;

  if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &event) == -1) {
    LOG_ERROR(EPOLL_MANAGER_COMPONENT, "Failed to add file descriptor {}: {}", fd, std::strerror(errno));
    return false;
  }

  return true;
}

/**
 * @brief Modifies the events for an existing file descriptor in the epoll instance.
 * @param fd The file descriptor to modify.
 * @param events The new events to monitor for the file descriptor.
 * @return True if the operation was successful, false otherwise.
 */
bool EpollManager::modify_fd(int fd, uint32_t events) {
  epoll_event event;
  event.events = events;
  event.data.fd = fd;

  if (epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &event) == -1) {
    LOG_ERROR(EPOLL_MANAGER_COMPONENT, "Failed to modify file descriptor {}: {}", fd, std::strerror(errno));
    return false;
  }

  return true;
}

/**
 * @brief Removes a file descriptor from the epoll instance.
 * @param fd The file descriptor to remove.
 * @return True if the operation was successful, false otherwise.
 */
bool EpollManager::remove_fd(int fd) {
  if (epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr) == -1) {
    LOG_ERROR(EPOLL_MANAGER_COMPONENT, "Failed to remove file descriptor {}: {}", fd, std::strerror(errno));
    return false;
  }

  return true;
}

/**
 * @brief Waits for events on the epoll instance.
 * @param timeout The maximum time to wait for events, in milliseconds.
 * @return The number of events that occurred, or -1 on error.
 */
int EpollManager::wait(int timeout) {
  int num_events = epoll_wait(epoll_fd_, events_.data(), events_.size(), timeout);
  if (num_events == -1) {
    LOG_ERROR(EPOLL_MANAGER_COMPONENT, "Epoll wait failed: {}", std::strerror(errno));
    return -1;
  }

  return num_events;
}

} // namespace server
} // namespace chat_app