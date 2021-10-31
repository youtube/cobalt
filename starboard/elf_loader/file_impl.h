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

#ifndef STARBOARD_ELF_LOADER_FILE_IMPL_H_
#define STARBOARD_ELF_LOADER_FILE_IMPL_H_

#include "starboard/elf_loader/file.h"

#include <string>

#include "starboard/file.h"

namespace starboard {
namespace elf_loader {

// Starboard implementation for reading a file.
class FileImpl : public File {
 public:
  FileImpl();
  ~FileImpl() override;
  bool Open(const char* name) override;
  bool ReadFromOffset(int64_t offset, char* buffer, int size) override;
  void Close() override;
  const std::string& GetName() override;

 protected:
  SbFile file_;
  std::string name_;

  FileImpl(const FileImpl&) = delete;
  void operator=(const FileImpl&) = delete;
};

}  // namespace elf_loader
}  // namespace starboard

#endif  // STARBOARD_ELF_LOADER_FILE_IMPL_H_
