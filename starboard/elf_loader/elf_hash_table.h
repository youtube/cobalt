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

#ifndef STARBOARD_ELF_LOADER_ELF_HASH_TABLE_H_
#define STARBOARD_ELF_LOADER_ELF_HASH_TABLE_H_

#include <stddef.h>
#include "starboard/elf_loader/elf.h"

namespace starboard {
namespace elf_loader {

// Models the hash table used to map symbol names to symbol entries using
// the standard ELF format.
class ElfHashTable {
 public:
  ElfHashTable();
  // Initialize instance. |dt_elf_hash| should be the address that the
  // DT_HASH entry points to in the input ELF dynamic section. Call IsValid()
  // to determine whether the table was well-formed.
  void Init(uintptr_t dt_elf_hash);

  // Returns true iff the content of the table is valid.
  bool IsValid() const;

  // Index of the first dynamic symbol within the ELF symbol table.
  size_t dyn_symbols_offset() const { return 1; }

  // Number of dynamic symbols in the ELF symbol table.
  size_t dyn_symbols_count() const { return hash_chain_size_ - 1; }

  // Lookup |symbol_name| in the table. |symbol_table| should point to the
  // ELF symbol table, and |string_table| to the start of its string table.
  // Returns NULL on failure.
  const Sym* LookupByName(const char* symbol_name,
                          const Sym* symbol_table,
                          const char* string_table) const;

 private:
  const Word* hash_bucket_;
  size_t hash_bucket_size_;
  const Word* hash_chain_;
  size_t hash_chain_size_;

  ElfHashTable(const ElfHashTable&) = delete;
  void operator=(const ElfHashTable&) = delete;
};

}  // namespace elf_loader
}  // namespace starboard

#endif  // STARBOARD_ELF_LOADER_ELF_HASH_TABLE_H_
