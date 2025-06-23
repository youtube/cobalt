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

#include "starboard/nplb/posix_compliance/posix_epoll_test_helper.h"

#include <unistd.h>

namespace starboard {
namespace nplb {

bool CreatePipe(int pipe_fds[2], int flags) {
  if (flags == 0) {
    return pipe(pipe_fds) == 0;
  }
  return pipe2(pipe_fds, flags) == 0;
}

}  // namespace nplb
}  // namespace starboard
