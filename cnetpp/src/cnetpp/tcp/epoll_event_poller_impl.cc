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
#if defined(linux) || defined(__linux) || defined(__linux__)
#include <cnetpp/tcp/epoll_event_poller_impl.h>
#include <cnetpp/tcp/event.h>
#include <cnetpp/tcp/event_center.h>
#include <cnetpp/concurrency/this_thread.h>
#include <cnetpp/base/log.h>

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

namespace cnetpp {
namespace tcp {

bool EpollEventPollerImpl::DoInit() {
  epoll_fd_ = ::epoll_create1(EPOLL_CLOEXEC);
  if (epoll_fd_ < 0) {
    Error("epoll_create1() failed. erro message: %s",
        concurrency::ThisThread::GetErrorString(
          concurrency::ThisThread::GetLastError()).c_str());
    return false;
  }
  return true;
}

bool EpollEventPollerImpl::Poll() {
  // before starting polling, we first process all the pending command events
  if (!ProcessInterrupt()) {
    return false;
  }

  int count { 0 };
  do {
    count = ::epoll_wait(epoll_fd_,
                         &epoll_events_[0],
                         epoll_events_.size(),
                         -1);
  } while (count == -1 &&
      cnetpp::concurrency::ThisThread::GetLastError() == EINTR);

  if (count < 0) {
    return false;
  }

  for (auto i = 0; i < count; ++i) {
    auto fd = epoll_events_[i].data.fd;
    if (fd == interrupter_->get_read_fd()) {
      // we have some command events to be processed
      //if (!ProcessInterrupt()) {
      //  return false;
      //}
    } else {
      Event event(fd);
      if (epoll_events_[i].events & (EPOLLHUP | EPOLLERR)) {
        event.mutable_mask() |= static_cast<int>(Event::Type::kClose);
      } else {
        if (epoll_events_[i].events & (EPOLLIN | EPOLLRDBAND | EPOLLRDNORM)) {
          event.mutable_mask() |= static_cast<int>(Event::Type::kRead);
        }
        if (epoll_events_[i].events & (EPOLLOUT | EPOLLWRNORM | EPOLLWRBAND)) {
          event.mutable_mask() |= static_cast<int>(Event::Type::kWrite);
        }
      }

      std::shared_ptr<EventCenter> event_center = event_center_.lock();
      if (!event_center || !event_center->ProcessEvent(event, id_)) {
        return false;
      }
    }
  }
  return true;
}

bool EpollEventPollerImpl::AddPollerEvent(Event&& ev) {
  struct epoll_event epoll_ev {0u, 0};
  epoll_ev.data.fd = ev.fd();
  epoll_ev.events = EPOLLIN;
  if (ev.mask() & static_cast<int>(Event::Type::kWrite)) {
    epoll_ev.events |= EPOLLOUT;
  }
  return ::epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, ev.fd(), &epoll_ev) == 0;
}

bool EpollEventPollerImpl::ModifyPollerEvent(Event&& ev) {
  struct epoll_event epoll_ev {0u, 0};
  epoll_ev.data.fd = ev.fd();
  epoll_ev.events = EPOLLIN;
  if (ev.mask() & static_cast<int>(Event::Type::kWrite)) {
    epoll_ev.events |= EPOLLOUT;
  }
  return ::epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, ev.fd(), &epoll_ev) == 0;
}

bool EpollEventPollerImpl::RemovePollerEvent(Event&& ev) {
  if (ev.mask() & static_cast<int>(Event::Type::kClose)) {
    return ::epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, ev.fd(), NULL) == 0;
  }
  return false;
}

}  // namespace tcp
}  // namespace cnetpp

#endif  // defined(linux) || defined(__linux) || defined(__linux__)

