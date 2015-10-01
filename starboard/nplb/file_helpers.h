// Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef STARBOARD_NBLP_FILE_HELPERS_H_
#define STARBOARD_NBLP_FILE_HELPERS_H_

#include <string>

#include "starboard/file.h"

namespace starboard {
namespace nplb {

// Creates a random file of the given length, and deletes it when the instance
// falls out of scope.
class ScopedRandomFile {
 public:
  // |length| is the length of the file created.
  // |create| is whether to create the file or not.
  ScopedRandomFile(int length, bool create = true) {
    filename_ = (create ? MakeRandomFile(length) : MakeRandomFilename());
  }

  ~ScopedRandomFile() {
    SbFileDelete(filename_.c_str());
  }

  const std::string &filename() const {
    return filename_;
  }

  // Checks |buffer| of size |size| against this class's write pattern, offset
  // by |pattern_offset|. Failures print the original line number |line|.
  static void ExpectPattern(
      int pattern_offset,
      void *buffer,
      int size,
      int line);

 private:
  // Creates a file with a random name and |length| bytes, returning the path to
  // the new file.
  static std::string MakeRandomFile(int length);

  // Creates and returns a random filename, but does not create the file.
  static std::string MakeRandomFilename();

  std::string filename_;
};

}  // namespace nplb
}  // namespace starboard

#endif
