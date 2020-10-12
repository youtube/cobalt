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

#ifndef STARBOARD_ELF_LOADER_ELF_LOADER_IMPL_H_
#define STARBOARD_ELF_LOADER_ELF_LOADER_IMPL_H_

#include "starboard/common/scoped_ptr.h"
#include "starboard/elf_loader/dynamic_section.h"
#include "starboard/elf_loader/elf.h"
#include "starboard/elf_loader/elf_hash_table.h"
#include "starboard/elf_loader/elf_header.h"
#include "starboard/elf_loader/exported_symbols.h"
#include "starboard/elf_loader/file.h"
#include "starboard/elf_loader/gnu_hash_table.h"
#include "starboard/elf_loader/program_table.h"
#include "starboard/elf_loader/relocations.h"

namespace starboard {
namespace elf_loader {

// Implementation of the elf loader.
class ElfLoaderImpl {
 public:
  ElfLoaderImpl();
  bool Load(const char* file_name,
            const void* (*custom_get_extension)(const char* name));
  void* LookupSymbol(const char* symbol);
  ~ElfLoaderImpl();

 private:
  scoped_ptr<File> elf_file_;
  scoped_ptr<ElfHeader> elf_header_loader_;
  scoped_ptr<ProgramTable> program_table_;
  scoped_ptr<DynamicSection> dynamic_section_;
  scoped_ptr<ExportedSymbols> exported_symbols_;
  scoped_ptr<Relocations> relocations_;

  ElfLoaderImpl(const ElfLoaderImpl&) = delete;
  void operator=(const ElfLoaderImpl&) = delete;
};

}  // namespace elf_loader
}  // namespace starboard
#endif  // STARBOARD_ELF_LOADER_ELF_LOADER_IMPL_H_
