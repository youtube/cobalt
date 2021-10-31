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

#ifndef STARBOARD_ELF_LOADER_DYNAMIC_SECTION_H_
#define STARBOARD_ELF_LOADER_DYNAMIC_SECTION_H_

#include "starboard/elf_loader/elf.h"
#include "starboard/elf_loader/elf_hash_table.h"
#include "starboard/elf_loader/exported_symbols.h"
#include "starboard/elf_loader/gnu_hash_table.h"
#include "starboard/elf_loader/program_table.h"

namespace starboard {
namespace elf_loader {

typedef void (*linker_function_t)();

// class representing the ELF dynamic
// section with dynamic symbols and a
// string tables.

class DynamicSection {
 public:
  DynamicSection(Addr base_memory_address,
                 Dyn* dynamic,
                 size_t dynamic_count,
                 Word dynamic_flags);

  // Initialize the dynamic section.
  bool InitDynamicSection();

  // Get pointer to the dynamic table.
  const Dyn* GetDynamicTable();

  // Get the size of the dynamic table
  size_t GetDynamicTableSize();

  // Call all the global constructors.
  void CallConstructors();

  // Call all the global destructors.
  void CallDestructors();

  // Lookup a symbol using its name.
  const Sym* LookupByName(const char* symbol_name) const;

  // Lookup a symbol using its id.
  const Sym* LookupById(size_t symbol_id) const;

  // Checks if a symbols is weak.
  bool IsWeakById(size_t symbol_id) const;

  // Lookup the name of a symbol by using its id.
  const char* LookupNameById(size_t symbol_id) const;

 private:
  // Call a function.
  void CallFunction(linker_function_t func, const char* func_type);

  Addr base_memory_address_;
  const char* soname_;

  Dyn* dynamic_;
  size_t dynamic_count_;
  Word dynamic_flags_;
  bool has_DT_SYMBOLIC_;

  Sym* symbol_table_;
  const char* string_table_;
  ElfHashTable elf_hash_;
  GnuHashTable gnu_hash_;

  linker_function_t* preinit_array_;
  size_t preinit_array_count_;
  linker_function_t* init_array_;
  size_t init_array_count_;
  linker_function_t* fini_array_;
  size_t fini_array_count_;
  linker_function_t init_func_;
  linker_function_t fini_func_;

  DynamicSection(const DynamicSection&) = delete;
  void operator=(const DynamicSection&) = delete;
};

}  // namespace elf_loader
}  // namespace starboard

#endif  // STARBOARD_ELF_LOADER_DYNAMIC_SECTION_H_
