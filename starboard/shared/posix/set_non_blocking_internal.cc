// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/posix/set_non_blocking_internal.h"

#include <fcntl.h>

namespace starboard {
namespace shared {
namespace posix {

bool SetNonBlocking(int socket_fd) {
  int flags = fcntl(socket_fd, F_GETFL, 0);
  return !(flags < 0 || fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK) == -1);
}

}  // namespace posix
}  // namespace shared
}  // namespace starboard
