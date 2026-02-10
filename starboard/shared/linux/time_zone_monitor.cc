// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "starboard/shared/linux/time_zone_monitor.h"

#include <poll.h>
#include <sys/eventfd.h>
#include <sys/inotify.h>
#include <unistd.h>
#include <memory>
#include <string>
#include <vector>

#include "starboard/common/log.h"
#include "starboard/shared/starboard/application.h"

namespace starboard {
namespace shared {
namespace linux {

namespace {
const char* const kFilesToWatch[] = {
    "/etc/localtime",
    "/etc/timezone",
    "/etc/TZ",
};
}  // namespace

// static
std::unique_ptr<TimeZoneMonitor> TimeZoneMonitor::Create() {
  std::unique_ptr<TimeZoneMonitor> monitor(new TimeZoneMonitor());
  if (monitor->inotify_fd_ == -1 || monitor->stop_event_fd_ == -1) {
    return nullptr;
  }
  monitor->Start();
  return monitor;
}

TimeZoneMonitor::TimeZoneMonitor() : ::starboard::Thread("TimeZoneMonitor") {
  inotify_fd_ = inotify_init1(IN_NONBLOCK);
  if (inotify_fd_ == -1) {
    SB_LOG(ERROR) << "TimeZoneMonitor failed to initialize inotify";
  }
  stop_event_fd_ = eventfd(0, 0);
  if (stop_event_fd_ == -1) {
    SB_LOG(ERROR) << "TimeZoneMonitor failed to initialize eventfd";
  }
}

TimeZoneMonitor::~TimeZoneMonitor() {
  stop_requested_ = true;
  if (stop_event_fd_ != -1) {
    uint64_t value = 1;
    ssize_t result = write(stop_event_fd_, &value, sizeof(value));
    if (result == -1) {
      SB_LOG(ERROR) << "TimeZoneMonitor failed to signal stop eventfd";
    }
  }
  Join();
  if (inotify_fd_ != -1) {
    close(inotify_fd_);
  }
  if (stop_event_fd_ != -1) {
    close(stop_event_fd_);
  }
}

void TimeZoneMonitor::Run() {
  SB_DCHECK(inotify_fd_ != -1);
  SB_DCHECK(stop_event_fd_ != -1);

  // Initial setup of watches.
  UpdateWatches();

  while (!stop_requested_) {
    struct pollfd fds[] = {
        {inotify_fd_, POLLIN, 0},
        {stop_event_fd_, POLLIN, 0},
    };
    int poll_result = poll(fds, 2, -1 /* infinite */);

    if (poll_result < 0) {
      if (errno == EINTR) {
        continue;
      }
      SB_LOG(ERROR) << "TimeZoneMonitor fatal poll error: " << errno;
      break;
    }

    // Handle stop events and check stop signal first to prioritize shutdown.
    if (stop_requested_ || (fds[1].revents & POLLIN)) {
      break;
    }

    if (fds[0].revents & POLLIN) {
      char buffer[4096]
          __attribute__((aligned(__alignof__(struct inotify_event))));
      ssize_t len = read(inotify_fd_, buffer, sizeof(buffer));

      if (len <= 0) {
        if (len < 0 && errno != EAGAIN && errno != EINTR) {
          SB_LOG(ERROR) << "TimeZoneMonitor failed to read";
        }
        continue;
      }

      bool changed = false;
      char* ptr = buffer;
      while (ptr < buffer + len) {
        const struct inotify_event* event =
            reinterpret_cast<const struct inotify_event*>(ptr);
        if (event->mask &
            (IN_MODIFY | IN_ATTRIB | IN_MOVE_SELF | IN_DELETE_SELF)) {
          changed = true;
          break;
        }
        ptr += sizeof(struct inotify_event) + event->len;
      }

      if (changed) {
        SB_LOG(INFO) << "Time zone file change detected, injecting event.";
        ::starboard::Application::Get()
            ->InjectDateTimeConfigurationChangedEvent();
        // Re-apply watches because tools like timedatectl replace the symlink,
        // which removes the inotify watch.
        UpdateWatches();
      }
    }
  }
}

void TimeZoneMonitor::UpdateWatches() {
  for (const char* path : kFilesToWatch) {
    // IN_DONT_FOLLOW is critical so we watch the symlink itself, not the
    // target.
    int wd = inotify_add_watch(
        inotify_fd_, path,
        IN_MODIFY | IN_ATTRIB | IN_MOVE_SELF | IN_DELETE_SELF | IN_DONT_FOLLOW);
    if (wd == -1) {
      if (errno != ENOENT) {
        SB_LOG(WARNING) << "Failed to watch " << path;
      }
    } else {
      SB_LOG(INFO) << "Monitoring " << path << " for time zone changes.";
    }
  }
}

}  // namespace linux
}  // namespace shared
}  // namespace starboard
