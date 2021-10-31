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

#include "starboard/elf_loader/gnu_hash_table.h"

#include <cstring>

#include "starboard/common/log.h"
#include "starboard/elf_loader/log.h"
#include "starboard/string.h"

namespace starboard {
namespace elf_loader {

// Compute the GNU hash of a given symbol.
// For more details on the hash function:
//  https://blogs.oracle.com/solaris/gnu-hash-elf-sections-v2
static uint32_t GnuHash(const char* name) {
  uint32_t h = 5381;
  const uint8_t* ptr = reinterpret_cast<const uint8_t*>(name);
  while (*ptr) {
    h = h * 33 + *ptr++;
  }
  return h;
}

GnuHashTable::GnuHashTable()
    : num_buckets_(0),
      sym_offset_(0),
      sym_count_(0),
      bloom_word_mask_(0),
      bloom_shift_(0),
      bloom_filter_(NULL),
      buckets_(NULL),
      chain_(NULL) {}
void GnuHashTable::Init(uintptr_t dt_gnu_hash) {
  SB_DLOG(INFO) << "GnuHashTable::Init 0x" << std::hex << dt_gnu_hash;
  sym_count_ = 0;

  const uint32_t* data = reinterpret_cast<const uint32_t*>(dt_gnu_hash);
  num_buckets_ = data[0];
  sym_offset_ = data[1];

  SB_DLOG(INFO) << "GnuHashTable::Init num_buckets_=" << num_buckets_
                << " sym_offset_" << sym_offset_;
  if (!num_buckets_)
    return;

  const uint32_t bloom_size = data[2];
  SB_DLOG(INFO) << "GnuHashTable::Init bloom_size=" << bloom_size;
  if ((bloom_size & (bloom_size - 1U)) != 0)  // must be a power of 2
    return;

  bloom_word_mask_ = bloom_size - 1U;
  bloom_shift_ = data[3];

  SB_DLOG(INFO) << "GnuHashTable::Init bloom_word_mask_=" << bloom_word_mask_;
  SB_DLOG(INFO) << "GnuHashTable::Init bloom_shift_=" << bloom_shift_;
  bloom_filter_ = reinterpret_cast<const Addr*>(data + 4);
  SB_DLOG(INFO) << "GnuHashTable::Init bloom_filter_=0x" << std::hex
                << bloom_filter_;
  buckets_ = reinterpret_cast<const uint32_t*>(bloom_filter_ + bloom_size);

  SB_DLOG(INFO) << "GnuHashTable::Init buckets_=0x" << std::hex << buckets_;
  chain_ = buckets_ + num_buckets_;

  // Compute number of dynamic symbols by parsing the table.
  if (num_buckets_ > 0) {
    // First find the maximum index in the buckets table.
    uint32_t max_index = buckets_[0];
    for (size_t n = 1; n < num_buckets_; ++n) {
      uint32_t sym_index = buckets_[n];
      if (sym_index > max_index)
        max_index = sym_index;
    }
    // Now start to look at the chain_ table from (max_index - sym_offset_)
    // until there is a value with LSB set to 1, indicating the end of the
    // last chain.
    while ((chain_[max_index - sym_offset_] & 1) == 0)
      max_index++;

    sym_count_ = (max_index - sym_offset_) + 1;
  }
}

bool GnuHashTable::IsValid() const {
  return sym_count_ > 0;
}

const Sym* GnuHashTable::LookupByName(const char* symbol_name,
                                      const Sym* symbol_table,
                                      const char* string_table) const {
  SB_DLOG(INFO) << "GnuHashTable::LookupByName: " << symbol_name;
  uint32_t hash = GnuHash(symbol_name);

  SB_DLOG(INFO) << "GnuHashTable::LookupByName: hash=" << hash;
  SB_DLOG(INFO) << "GnuHashTable::LookupByName: ELF_BITS=" << ELF_BITS;

  // First, bloom filter test.
  Addr word = bloom_filter_[(hash / ELF_BITS) & bloom_word_mask_];

  SB_DLOG(INFO) << "GnuHashTable::LookupByName: word=" << word;
  Addr mask = (Addr(1) << (hash % ELF_BITS)) |
              (Addr(1) << ((hash >> bloom_shift_) % ELF_BITS));

  SB_DLOG(INFO) << "GnuHashTable::LookupByName: mask=" << mask;
  if ((word & mask) != mask)
    return NULL;

  uint32_t sym_index = buckets_[hash % num_buckets_];
  if (sym_index < sym_offset_)
    return NULL;

  // TODO: add validations of the syn_index
  while (true) {
    const Sym* sym = symbol_table + sym_index;
    const uint32_t sym_hash = chain_[sym_index - sym_offset_];
    const char* sym_name = string_table + sym->st_name;

    if ((sym_hash | 1) == (hash | 1) && !strcmp(sym_name, symbol_name)) {
      return sym;
    }

    if (sym_hash & 1)
      break;

    sym_index++;
  }

  return NULL;
}

}  // namespace elf_loader
}  // namespace starboard
