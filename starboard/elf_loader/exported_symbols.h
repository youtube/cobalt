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

#ifndef STARBOARD_ELF_LOADER_EXPORTED_SYMBOLS_H_
#define STARBOARD_ELF_LOADER_EXPORTED_SYMBOLS_H_

#include <map>
#include <string>

#include "starboard/elf_loader/elf_hash_table.h"
#include "starboard/elf_loader/gnu_hash_table.h"
#include "starboard/file.h"

namespace starboard {
namespace elf_loader {

// class representing all exported symbols
// by the starboard layer.

// The elf_loader will not use any other symbols
// outside of the set represented in this class.
class ExportedSymbols {
 public:
  // An optional |custom_get_extension| function pointer can be passed in order
  // to override the |SbSystemGetExtension| function.
  explicit ExportedSymbols(
      const void* (*custom_get_extension)(const char* name) = NULL);
  const void* Lookup(const char* name);

 private:
  std::map<std::string, const void*> map_;

  ExportedSymbols(const ExportedSymbols&) = delete;
  void operator=(const ExportedSymbols&) = delete;
};

}  // namespace elf_loader
}  // namespace starboard
#endif  // STARBOARD_ELF_LOADER_EXPORTED_SYMBOLS_H_
