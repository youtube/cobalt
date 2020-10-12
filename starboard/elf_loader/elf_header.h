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

#ifndef STARBOARD_ELF_LOADER_ELF_HEADER_H_
#define STARBOARD_ELF_LOADER_ELF_HEADER_H_

#include "starboard/elf_loader/elf.h"

#include "starboard/common/scoped_ptr.h"
#include "starboard/elf_loader/file.h"

namespace starboard {
namespace elf_loader {

// Class for loading, parsing and validating the ELF header section.
class ElfHeader {
 public:
  ElfHeader();

  // Load, parse and validate the ELF header.
  bool LoadElfHeader(File* file);

  // Get the actual header data.
  const Ehdr* GetHeader();

 private:
  scoped_ptr<Ehdr> elf_header_;

  ElfHeader(const ElfHeader&) = delete;
  void operator=(const ElfHeader&) = delete;
};

}  // namespace elf_loader
}  // namespace starboard

#endif  // STARBOARD_ELF_LOADER_ELF_HEADER_H_
