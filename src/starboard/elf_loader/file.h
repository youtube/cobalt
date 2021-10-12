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

#ifndef STARBOARD_ELF_LOADER_FILE_H_
#define STARBOARD_ELF_LOADER_FILE_H_

#include <string>

#include "starboard/types.h"

namespace starboard {
namespace elf_loader {

// File abstraction to be used by the ELF loader.
// The main reason to introduce this class is to allow for
// easy testing.
class File {
 public:
  // Opens the file specified for reading.
  virtual bool Open(const char* name) = 0;

  // Reads a buffer from the file using the specified offset from the beginning
  // of the file.
  virtual bool ReadFromOffset(int64_t offset, char* buffer, int size) = 0;

  // Closes the underlying file.
  virtual void Close() = 0;

  // Returns the name of the file.
  virtual const std::string& GetName() = 0;

  virtual ~File() {}
};

}  // namespace elf_loader
}  // namespace starboard

#endif  // STARBOARD_ELF_LOADER_FILE_H_
