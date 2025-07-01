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

#include <poll.h>

#include "starboard/common/log.h"

SB_EXPORT int __abi_wrap_poll(struct musl_pollfd* poll_fd,
                              musl_nfds_t nfds,
                              int32_t n) {
  if (!poll_fd) {
    return 0;
  }
  struct pollfd pfd;
  pfd.fd = poll_fd->fd;
  pfd.events = poll_fd->events;
  pfd.revents = poll_fd->revents;

  int retval = poll(&pfd, nfds, n);

  poll_fd->fd = pfd.fd;
  poll_fd->events = pfd.events;
  poll_fd->revents = pfd.revents;

  return retval;
}
