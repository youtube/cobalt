// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

// Module Overview: Starboard Socket module C++ convenience layer
//
// Implements a convenience class that builds on top of the core Starboard
// socket functions.

#ifndef STARBOARD_COMMON_SOCKET_H_
#define STARBOARD_COMMON_SOCKET_H_

#include <ostream>

#include "starboard/socket.h"
#include "starboard/types.h"

namespace starboard {}  // namespace starboard

// Let SbSocketAddresses be output to log streams.
std::ostream& operator<<(std::ostream& os, const SbSocketAddress& address);

#endif  // STARBOARD_COMMON_SOCKET_H_
