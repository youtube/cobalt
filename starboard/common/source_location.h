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

// Minimalistic SourceLocation implementation, because the C++ code style
// bans the use of std::source_location due to duplication with abseil, but in
// Starboard we don't have abseil.

#ifndef STARBOARD_COMMON_SOURCE_LOCATION_H_
#define STARBOARD_COMMON_SOURCE_LOCATION_H_

#include <ostream>

namespace starboard {

class SourceLocation {
 public:
  static constexpr SourceLocation current(int line = __builtin_LINE(),
                                          const char* file = __builtin_FILE()) {
    return SourceLocation(line, file);
  }
  constexpr int line() const { return line_; }
  constexpr const char* file() const { return file_; }

  friend std::ostream& operator<<(std::ostream& out,
                                  const SourceLocation& location);

 private:
  constexpr SourceLocation(int line, const char* file)
      : line_(line), file_(file) {}

  int line_ = 0;
  const char* file_ = nullptr;
};

}  // namespace starboard

#endif  // STARBOARD_COMMON_SOURCE_LOCATION_H_
