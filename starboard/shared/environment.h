// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

// Module Overview: Starboard Byte Swap module C++ convenience layer
//
// Implements convenient templated wrappers around the core Starboard Byte Swap
// module. These functions are used to deal with endianness when performing I/O.

#ifndef STARBOARD_SHARED_ENVIRONMENT_H_
#define STARBOARD_SHARED_ENVIRONMENT_H_

#include <string>

namespace starboard {

std::string GetEnvironment(std::string name);

}  // namespace starboard

#endif  // STARBOARD_SHARED_ENVIRONMENT_H_
