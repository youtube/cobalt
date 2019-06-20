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

#ifndef STARBOARD_ELF_LOADER_ELF_LOADER_H_
#define STARBOARD_ELF_LOADER_ELF_LOADER_H_

#include "starboard/common/scoped_ptr.h"

namespace starboard {
namespace elf_loader {

class ElfLoaderImpl;

// A loader for ELF dynamic shared library.
class ElfLoader {
 public:
  ElfLoader();

  // Loads the shared library
  bool Load(const char* file_name);

  // Looks up the symbol address in the
  // shared library.
  void* LookupSymbol(const char* symbol);

  ~ElfLoader();

 private:
  scoped_ptr<ElfLoaderImpl> impl_;

  SB_DISALLOW_COPY_AND_ASSIGN(ElfLoader);
};

}  // namespace elf_loader
}  // namespace starboard
#endif  // STARBOARD_ELF_LOADER_ELF_LOADER_H_
