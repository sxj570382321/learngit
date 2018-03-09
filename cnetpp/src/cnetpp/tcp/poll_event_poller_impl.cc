// Copyright (c) 2015, myjfm(mwxjmmyjfm@gmail.com).  All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//   * Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//   * Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation
// and/or other materials provided with the distribution.
//   * Neither the name of myjfm nor the names of other contributors may be
// used to endorse or promote products derived from this software without
// specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT(INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
#if defined(linux) || defined(__linux) || defined(__linux__) || \
  defined(macintosh) || defined(__APPLE__) || defined(__APPLE_CC__)

#include <cnetpp/tcp/poll_event_poller_impl.h>
#include <cnetpp/tcp/event_center.h>
#include <cnetpp/tcp/event.h>
#include <cnetpp/tcp/interrupter.h>
#include <cnetpp/concurrency/this_thread.h>

#include <errno.h>
#include <fcntl.h>
#include <sys/select.h>
#include <unistd.h>

namespace cnetpp {
namespace tcp {

bool PollEventPollerImpl::Poll() {
  // before starting polling, we first process all the pending command events
  if (!ProcessInterrupt()) {
    return false;
  }

  // should not happen
  // because we have at least one fd in the fd sets(pipe read fd)
  if (poll_fds_end_ == 0) {
    return true;
  }

  int count = 0;
  do {
    count = ::poll(&(poll_fds_[0]), poll_fds_end_, -1);
  } while (count == -1 &&
           cnetpp::concurrency::ThisThread::GetLastError() == EINTR);

  if (count < 0) {
    return false;
  }

  auto event_center = event_center_.lock();
  if (!event_center.get()) {
    return false;
  }

  int end = poll_fds_end_;
  for (int i = 0; i < end; ++i) {
    int fd = poll_fds_[i].fd;
    // waked up by interrupter
    if (fd == interrupter_->get_read_fd()) {
      //if (!ProcessInterrupt()) {
      //  return false;
      //}
      continue;
    }

    bool has_event = false;
    Event event(fd);
    int revents = poll_fds_[i].revents;

    if (revents & (POLLERR | POLLHUP | POLLNVAL)) {
      has_event = true;
      event.mutable_mask() |= static_cast<int>(Event::Type::kClose);
    } else {
      if (revents & (POLLIN | POLLRDNORM | POLLPRI | POLLRDBAND)) {
        has_event = true;
        event.mutable_mask() |= static_cast<int>(Event::Type::kRead);
      }
      if (revents & (POLLOUT | POLLWRNORM | POLLWRBAND)) {
        has_event = true;
        event.mutable_mask() |= static_cast<int>(Event::Type::kWrite);
      }
    }

    if (has_event && !event_center->ProcessEvent(event, id_)) {
      return false;
    }
  }

  InternalRemovePollerEvent();

  return true;
}

bool PollEventPollerImpl::AddPollerEvent(Event&& event) {
  if (static_cast<size_t>(poll_fds_end_) >= max_connections_) {
    // TODO log an error
    // we are too many connections, just close the new arrival
    base::TcpSocket socket;
    socket.Attach(event.fd());
    socket.Close();
    return false;
  }
  poll_fds_[poll_fds_end_].fd = event.fd();
  poll_fds_[poll_fds_end_].events = 0;
  if (event.mask() & static_cast<int>(Event::Type::kRead)) {
    poll_fds_[poll_fds_end_].events |= POLLIN;
  }
  if (event.mask() & static_cast<int>(Event::Type::kWrite)) {
    poll_fds_[poll_fds_end_].events |= POLLOUT;
  }
  fd_to_index_map_[event.fd()] = poll_fds_end_++;
  return true;
}

bool PollEventPollerImpl::ModifyPollerEvent(Event&& event) {
  auto itr = fd_to_index_map_.find(event.fd());
  assert(itr != fd_to_index_map_.end());
  auto index = itr->second;
  assert(poll_fds_[index].fd == event.fd());
  poll_fds_[index].events = 0;
  if (event.mask() & static_cast<int>(Event::Type::kRead)) {
    poll_fds_[index].events |= POLLIN;
  }
  if (event.mask() & static_cast<int>(Event::Type::kWrite)) {
    poll_fds_[index].events |= POLLOUT;
  }
  return true;
}

bool PollEventPollerImpl::RemovePollerEvent(Event&& event) {
  events_to_be_removed_.emplace_back(std::move(event));
  return true;
}

void PollEventPollerImpl::InternalRemovePollerEvent() {
  for (auto& event : events_to_be_removed_) {
    auto itr = fd_to_index_map_.find(event.fd());
    assert(itr != fd_to_index_map_.end());
    auto index = itr->second;
    assert(poll_fds_[index].fd == event.fd());
    poll_fds_end_--;
    fd_to_index_map_.erase(itr);
    if (index == poll_fds_end_) {
      continue;
    }
    poll_fds_[index].fd = poll_fds_[poll_fds_end_].fd;
    poll_fds_[index].events = poll_fds_[poll_fds_end_].events;
    poll_fds_[index].revents = poll_fds_[poll_fds_end_].revents;
    fd_to_index_map_[poll_fds_[index].fd] = index;
  }
  events_to_be_removed_.clear();
}

}  // namespace tcp
}  // namespace cnetpp

#endif  // system related macros check
