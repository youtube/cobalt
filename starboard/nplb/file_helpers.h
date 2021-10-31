// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_NPLB_FILE_HELPERS_H_
#define STARBOARD_NPLB_FILE_HELPERS_H_

#include <string>
#include <vector>

#include "starboard/file.h"

namespace starboard {
namespace nplb {

// Gets the temporary directory in which ScopedRandomFile places its files.
std::string GetTempDir();

// Gets the directory in which static data for files tests is stored.
std::string GetFileTestsDataDir();

// Gets the paths of files in the static data for files tests.
std::vector<std::string> GetFileTestsFilePaths();

// Gets the paths of directories in the static data for files tests.
std::vector<std::string> GetFileTestsDirectoryPaths();

// Gets the expected content in a static data test file.
std::string GetTestFileExpectedContent(const std::string& path);

// Creates a random file of the given length, and deletes it when the instance
// falls out of scope.
class ScopedRandomFile {
 public:
  enum {
    kDefaultLength = 64,
  };

  enum Create {
    kCreate,
    kDontCreate,
  };

  // Will create a file of |kDefaultLength| bytes long.
  ScopedRandomFile() : size_(kDefaultLength) {
    filename_ = MakeRandomFile(size_);
  }

  // Will create a file |length| bytes long.
  explicit ScopedRandomFile(int length) : size_(length) {
    filename_ = MakeRandomFile(size_);
  }

  // Will either create a file |length| bytes long, or will just generate a
  // filename.  |create| is whether to create the file or not.
  ScopedRandomFile(int length, Create create) : size_(length) {
    filename_ =
        (create == kCreate ? MakeRandomFile(size_) : MakeRandomFilePath());
  }

  // Will either create a file of |kDefaultLength| bytes long, or will just
  // generate a filename.  |create| is whether to create the file or not.
  explicit ScopedRandomFile(Create create) : size_(kDefaultLength) {
    filename_ =
        (create == kCreate ? MakeRandomFile(size_) : MakeRandomFilePath());
  }

  ~ScopedRandomFile() { SbFileDelete(filename_.c_str()); }

  // Creates and returns a random filename (no path), but does not create the
  // file.
  static std::string MakeRandomFilename();

  // Returns the filename generated for this file.
  const std::string& filename() const { return filename_; }

  // Returns the SPECIFIED size of the file (not the size returned by the
  // filesystem).
  const int size() const { return size_; }

  // Checks |buffer| of size |size| against this class's write pattern, offset
  // by |pattern_offset|. Failures print the original line number |line|.
  static void ExpectPattern(int pattern_offset,
                            void* buffer,
                            int size,
                            int line);

 private:
  // Creates a file with a random name and |length| bytes, returning the path to
  // the new file.
  static std::string MakeRandomFile(int length);

  // Creates and returns a path to a random file, but does not create the file.
  static std::string MakeRandomFilePath();

  std::string filename_;
  int size_;
};

}  // namespace nplb
}  // namespace starboard

#endif  // STARBOARD_NPLB_FILE_HELPERS_H_
