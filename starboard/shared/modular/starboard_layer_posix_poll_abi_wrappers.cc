// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/modular/starboard_layer_posix_poll_abi_wrappers.h"

#include <errno.h>
#include <poll.h>

SB_EXPORT int __abi_wrap_poll(struct musl_pollfd* poll_fd,
                              musl_nfds_t nfds,
                              int32_t timeout) {
  if (!poll_fd) {
    return poll(nullptr, nfds, timeout);
  }

  struct pollfd fds[nfds];

  for (musl_nfds_t i = 0; i < nfds; i++) {
    fds[i].fd = poll_fd[i].fd;
    fds[i].events = poll_fd[i].events;
    fds[i].revents = 0;  // Clear revents before calling poll.
  }

  int retval = poll(fds, nfds, timeout);

  // If poll() was successful, copy the resulting revents
  // back to the original musl_pollfd array.
  if (retval >= 0) {
    for (musl_nfds_t i = 0; i < nfds; i++) {
      poll_fd[i].revents = fds[i].revents;
    }
  }

  return retval;
}
