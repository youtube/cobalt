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

#ifndef THIRD_PARTY_MUSL_SRC_STARBOARD_POSIX_SOCKET_H_
#define THIRD_PARTY_MUSL_SRC_STARBOARD_POSIX_SOCKET_H_

#include <stdint.h>
#include "starboard/socket.h"

#define PTHREAD_CREATE_JOINABLE 0
#define PTHREAD_CREATE_DETACHED 1

#ifdef __cplusplus
extern "C" {
#endif

int posixSocketGetFdFromSb(SbSocket socket);
SbSocket posixSocketGetSbFromFd(int socket);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // THIRD_PARTY_MUSL_SRC_STARBOARD_POSIX_SOCKET_H_
