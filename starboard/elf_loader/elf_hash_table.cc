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

#include "starboard/elf_loader/elf_hash_table.h"

#include <cstring>

#include "starboard/string.h"

namespace starboard {
namespace elf_loader {

// Compute the ELF hash of a given symbol.
// Defined in
// https://refspecs.linuxfoundation.org/elf/gabi4+/ch5.dynamic.html#hash
static unsigned ElfHash(const char* name) {
  const uint8_t* ptr = reinterpret_cast<const uint8_t*>(name);
  unsigned h = 0;
  while (*ptr) {
    h = (h << 4) + *ptr++;
    unsigned g = h & 0xf0000000;
    h ^= g;
    h ^= g >> 24;
  }
  return h;
}

ElfHashTable::ElfHashTable()
    : hash_bucket_(NULL),
      hash_bucket_size_(0),
      hash_chain_(NULL),
      hash_chain_size_(0) {}
void ElfHashTable::Init(uintptr_t dt_elf_hash) {
  const Word* data = reinterpret_cast<const Word*>(dt_elf_hash);
  hash_bucket_size_ = data[0];
  hash_bucket_ = data + 2;
  hash_chain_size_ = data[1];
  hash_chain_ = hash_bucket_ + hash_bucket_size_;
}

bool ElfHashTable::IsValid() const {
  return hash_bucket_size_ > 0;
}

const Sym* ElfHashTable::LookupByName(const char* symbol_name,
                                      const Sym* symbol_table,
                                      const char* string_table) const {
  unsigned hash = ElfHash(symbol_name);

  for (unsigned n = hash_bucket_[hash % hash_bucket_size_]; n != 0;
       n = hash_chain_[n]) {
    const Sym* symbol = &symbol_table[n];
    // Check that the symbol has the appropriate name.
    if (!strcmp(string_table + symbol->st_name, symbol_name))
      return symbol;
  }
  return NULL;
}

}  // namespace elf_loader
}  // namespace starboard
