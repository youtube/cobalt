// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_WIN32_POSIX_EMU_INCLUDE_SYS_SOCKET_H_
#define STARBOARD_SHARED_WIN32_POSIX_EMU_INCLUDE_SYS_SOCKET_H_

#include <winsock2.h>

#include "starboard/common/log.h"
#include "starboard/types.h"

#ifdef __cplusplus
extern "C" {
#endif

int sb_socket(int domain, int type, int protocol);
#define socket sb_socket

#ifdef __cplusplus
}
#endif
#endif  // STARBOARD_SHARED_WIN32_POSIX_EMU_INCLUDE_SYS_SOCKET_H_
