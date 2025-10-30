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

#include <sys/ioctl.h>

int ioctl_TIOCGWINSZ(int fd, struct winsize *wz) {
  // Return the current terminal size.
  // Note: In POSIX 2024, this can be handled by tcgetwinsize.
  if (wz) {
    wz->ws_row = 24;
    wz->ws_col = 80;
  }

  // Return error when the fd is not for a tty.
  return 0; // TEMPORARY TO FORCE LINE BUFFERING, was: isatty(fd) ? 0 : -1;
}
