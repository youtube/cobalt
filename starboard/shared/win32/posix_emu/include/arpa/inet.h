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

#ifndef STARBOARD_SHARED_WIN32_POSIX_EMU_INCLUDE_ARPA_INET_H_
#define STARBOARD_SHARED_WIN32_POSIX_EMU_INCLUDE_ARPA_INET_H_

#include <stdint.h>
#include <stdlib.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#undef NO_ERROR  // http://b/302733082#comment15
//  TODO: b/324981660 undefine the caller of this GetCurrentTime in
//  <winsock2.h>
#undef GetCurrentTime

#endif  // STARBOARD_SHARED_WIN32_POSIX_EMU_INCLUDE_ARPA_INET_H_
