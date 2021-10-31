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

#ifndef STARBOARD_ELF_LOADER_RELOCATIONS_H_
#define STARBOARD_ELF_LOADER_RELOCATIONS_H_

#include "starboard/elf_loader/elf.h"

#include "starboard/elf_loader/dynamic_section.h"
#include "starboard/elf_loader/program_table.h"

namespace starboard {
namespace elf_loader {

enum RelocationType {
  RELOCATION_TYPE_UNKNOWN = 0,
  RELOCATION_TYPE_ABSOLUTE = 1,
  RELOCATION_TYPE_RELATIVE = 2,
  RELOCATION_TYPE_PC_RELATIVE = 3,
  RELOCATION_TYPE_COPY = 4,
};

// class representing the ELF relocations.
class Relocations {
 public:
  Relocations(Addr base_memory_address,
              DynamicSection* dynamic_section,
              ExportedSymbols* exported_symbols);

  // Initialize the relocation tables.
  bool InitRelocations();

  // Apply all the relocations.
  bool ApplyAllRelocations();

  // Apply a set of relocations.
  bool ApplyRelocations(const rel_t* rel, size_t rel_count);

  // Apply an individual relocation.
  bool ApplyRelocation(const rel_t* rel);

// Apply a resolved symbol relocation.
#if defined(USE_RELA)
  bool ApplyResolvedReloc(const Rela* rela, Addr sym_addr);
#else
  bool ApplyResolvedReloc(const Rel* rel, Addr sym_addr);
#endif

  // Convert an ELF relocation type info a RelocationType value.
  RelocationType GetRelocationType(Word r_type);

  // Resolve a symbol address.
  bool ResolveSymbol(Word rel_type,
                     Word rel_symbol,
                     Addr reloc,
                     Addr* sym_addr);

  // Checks if there are any text relocations.
  bool HasTextRelocations();

 private:
  Addr base_memory_address_;
  DynamicSection* dynamic_section_;
  Addr plt_relocations_;
  size_t plt_relocations_size_;
  Addr* plt_got_;

  Addr relocations_;
  size_t relocations_size_;

  bool has_text_relocations_;
  bool has_symbolic_;

  ExportedSymbols* exported_symbols_;

  Relocations(const Relocations&) = delete;
  void operator=(const Relocations&) = delete;
};

}  // namespace elf_loader
}  // namespace starboard

#endif  // STARBOARD_ELF_LOADER_RELOCATIONS_H_
