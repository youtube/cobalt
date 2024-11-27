// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_COMMON_PATHS_H_
#define STARBOARD_COMMON_PATHS_H_

#include <string>

namespace starboard {
namespace common {

// Returns an empty string on error.
std::string PrependContentPath(const std::string& path);

// Returns the absolute path to a directory that contains Cobalt's trusted
// Certificate Authority (CA) root certificates, or an empty string if it can't
// be found.
std::string GetCACertificatesPath();
std::string GetCACertificatesPath(const std::string& content_subdir);

}  // namespace common
}  // namespace starboard

#endif  // STARBOARD_COMMON_PATHS_H_
