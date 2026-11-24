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

#include "starboard/elf_loader/dynamic_section.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "testing/gtest/include/gtest/gtest.h"

namespace elf_loader {

namespace {

typedef struct DynamicSectionMemory {
  Dyn dynamic_table_[3];
  linker_function_t init_array_[2];
  linker_function_t fini_array_[2];
  Sym symbol_table_[2];
  char string_table_[128];
} DynamicSectionMemory;

bool gInitCalled = false;
bool gCtor1Called = false;
bool gCtor2Called = false;
bool gDtor1Called = false;
bool gDtor2Called = false;

class DynamicSectionTest : public ::testing::Test {
 protected:
  DynamicSectionTest() {}
  ~DynamicSectionTest() {}

  void SetUp() override {
    gInitCalled = false;
    gCtor1Called = false;
    gCtor2Called = false;
    gDtor1Called = false;
    gDtor2Called = false;
  }
};

uint32_t ComputeGnuHash(const char* name) {
  uint32_t h = 5381;
  const uint8_t* ptr = reinterpret_cast<const uint8_t*>(name);
  while (*ptr) {
    h = h * 33 + *ptr++;
  }
  return h;
}

void InitFunction() {
  gInitCalled = true;
}

void TestConstructor1() {
  gCtor1Called = true;
}

void TestConstructor2() {
  gCtor2Called = true;
}

void TestDestructor1() {
  gDtor1Called = true;
}

void TestDestructor2() {
  gDtor2Called = true;
}

TEST_F(DynamicSectionTest, Initialize) {
  Addr addr = 0x1234;
  Dyn dynamic_table[3] = {};

  DynamicSection d(addr, dynamic_table, 3);
  ASSERT_TRUE(d.InitDynamicSection());

  ASSERT_EQ(dynamic_table, d.GetDynamicTable());
  ASSERT_EQ(3, d.GetDynamicTableSize());
}

TEST_F(DynamicSectionTest, CallInit) {
  DynamicSectionMemory dynamic_section_memory = {};
  Addr base_addr = reinterpret_cast<Addr>(&dynamic_section_memory);
  dynamic_section_memory.dynamic_table_[0].d_tag = DT_INIT;
  dynamic_section_memory.dynamic_table_[0].d_un.d_ptr =
      reinterpret_cast<Addr>(InitFunction) - base_addr;

  DynamicSection d(base_addr, dynamic_section_memory.dynamic_table_, 1);
  ASSERT_TRUE(d.InitDynamicSection());

  ASSERT_EQ(dynamic_section_memory.dynamic_table_, d.GetDynamicTable());
  ASSERT_EQ(1, d.GetDynamicTableSize());
  d.CallConstructors();
  ASSERT_TRUE(gInitCalled);
}

TEST_F(DynamicSectionTest, CallConstructors) {
  DynamicSectionMemory dynamic_section_memory = {};
  Addr base_addr = reinterpret_cast<Addr>(&dynamic_section_memory);

  dynamic_section_memory.init_array_[0] =
      reinterpret_cast<linker_function_t>(&TestConstructor1);
  dynamic_section_memory.init_array_[1] =
      reinterpret_cast<linker_function_t>(&TestConstructor2);

  dynamic_section_memory.dynamic_table_[0].d_tag = DT_INIT_ARRAY;
  dynamic_section_memory.dynamic_table_[0].d_un.d_ptr =
      reinterpret_cast<Addr>(dynamic_section_memory.init_array_) - base_addr;

  dynamic_section_memory.dynamic_table_[1].d_tag = DT_INIT_ARRAYSZ;
  dynamic_section_memory.dynamic_table_[1].d_un.d_val =
      sizeof(dynamic_section_memory.init_array_);

  DynamicSection d(base_addr, dynamic_section_memory.dynamic_table_, 2);
  ASSERT_TRUE(d.InitDynamicSection());

  ASSERT_EQ(dynamic_section_memory.dynamic_table_, d.GetDynamicTable());
  ASSERT_EQ(2, d.GetDynamicTableSize());
  d.CallConstructors();
  ASSERT_TRUE(gCtor1Called);
  ASSERT_TRUE(gCtor2Called);
}

TEST_F(DynamicSectionTest, CallDestructors) {
  DynamicSectionMemory dynamic_section_memory = {};
  Addr base_addr = reinterpret_cast<Addr>(&dynamic_section_memory);

  dynamic_section_memory.fini_array_[0] =
      reinterpret_cast<linker_function_t>(&TestDestructor1);
  dynamic_section_memory.fini_array_[1] =
      reinterpret_cast<linker_function_t>(&TestDestructor2);

  dynamic_section_memory.dynamic_table_[0].d_tag = DT_FINI_ARRAY;
  dynamic_section_memory.dynamic_table_[0].d_un.d_ptr =
      reinterpret_cast<Addr>(dynamic_section_memory.fini_array_) - base_addr;

  dynamic_section_memory.dynamic_table_[1].d_tag = DT_FINI_ARRAYSZ;
  dynamic_section_memory.dynamic_table_[1].d_un.d_val =
      sizeof(dynamic_section_memory.fini_array_);

  DynamicSection d(base_addr, dynamic_section_memory.dynamic_table_, 2);
  ASSERT_TRUE(d.InitDynamicSection());

  ASSERT_EQ(dynamic_section_memory.dynamic_table_, d.GetDynamicTable());
  ASSERT_EQ(2, d.GetDynamicTableSize());
  d.CallDestructors();
  ASSERT_TRUE(gDtor1Called);
  ASSERT_TRUE(gDtor2Called);
}

TEST_F(DynamicSectionTest, LookupById) {
  DynamicSectionMemory dynamic_section_memory = {};
  Addr base_addr = reinterpret_cast<Addr>(&dynamic_section_memory);

  dynamic_section_memory.dynamic_table_[0].d_tag = DT_SYMTAB;
  dynamic_section_memory.dynamic_table_[0].d_un.d_ptr =
      reinterpret_cast<Addr>(dynamic_section_memory.symbol_table_) - base_addr;

  DynamicSection d(base_addr, dynamic_section_memory.dynamic_table_, 1);

  ASSERT_TRUE(d.InitDynamicSection());

  ASSERT_EQ(dynamic_section_memory.dynamic_table_, d.GetDynamicTable());
  ASSERT_EQ(1, d.GetDynamicTableSize());
  ASSERT_EQ(dynamic_section_memory.symbol_table_, d.LookupById(0));
  ASSERT_EQ(dynamic_section_memory.symbol_table_ + 1, d.LookupById(1));
}

TEST_F(DynamicSectionTest, LookupNameById) {
  DynamicSectionMemory dynamic_section_memory = {};
  Addr base_addr = reinterpret_cast<Addr>(&dynamic_section_memory);

  dynamic_section_memory.dynamic_table_[0].d_tag = DT_SYMTAB;
  dynamic_section_memory.dynamic_table_[0].d_un.d_ptr =
      reinterpret_cast<Addr>(dynamic_section_memory.symbol_table_) - base_addr;

  dynamic_section_memory.dynamic_table_[1].d_tag = DT_STRTAB;
  dynamic_section_memory.dynamic_table_[1].d_un.d_ptr =
      reinterpret_cast<Addr>(dynamic_section_memory.string_table_) - base_addr;

  memcpy(dynamic_section_memory.string_table_, "test1\x00test2\x00", 12);

  dynamic_section_memory.symbol_table_[0].st_name = 0;  // the offset of test1
  dynamic_section_memory.symbol_table_[1].st_name = 6;  // the offset of test2

  DynamicSection d(base_addr, dynamic_section_memory.dynamic_table_, 2);

  ASSERT_TRUE(d.InitDynamicSection());

  ASSERT_EQ(dynamic_section_memory.dynamic_table_, d.GetDynamicTable());
  ASSERT_EQ(2, d.GetDynamicTableSize());

  ASSERT_EQ(0, strcmp("test1", d.LookupNameById(0)));
  ASSERT_EQ(0, strcmp("test2", d.LookupNameById(1)));
}

// When loading an ELF shared library whose dynamic table pointer is null,
// initialization must fail cleanly rather than dereferencing null memory.
TEST_F(DynamicSectionTest, InitFailsOnNullDynamicTable) {
  DynamicSection d(0x1234, nullptr, 3);
  EXPECT_FALSE(d.InitDynamicSection());
}

// When an ELF dynamic table contains a DT_NULL terminator before dynamic_count
// entries (such as linker-padded PT_DYNAMIC segments), entries after DT_NULL
// must be ignored so stale or padding entries cannot trigger unintended hooks.
TEST_F(DynamicSectionTest, InitStopsAtDtNull) {
  DynamicSectionMemory dynamic_section_memory = {};
  Addr base_addr = reinterpret_cast<Addr>(&dynamic_section_memory);

  dynamic_section_memory.dynamic_table_[0].d_tag = DT_NULL;
  dynamic_section_memory.dynamic_table_[1].d_tag = DT_INIT;
  dynamic_section_memory.dynamic_table_[1].d_un.d_ptr =
      reinterpret_cast<Addr>(InitFunction) - base_addr;

  DynamicSection d(base_addr, dynamic_section_memory.dynamic_table_, 2);
  ASSERT_TRUE(d.InitDynamicSection());
  d.CallConstructors();
  EXPECT_FALSE(gInitCalled);
}

// When a malformed ELF specifies DT_INIT_ARRAYSZ or DT_FINI_ARRAYSZ without
// the corresponding DT_INIT_ARRAY or DT_FINI_ARRAY pointer, running module
// constructors and destructors must not dereference a null array pointer.
TEST_F(DynamicSectionTest, CallConstructorsAndDestructorsIgnoreMissingArrays) {
  DynamicSectionMemory dynamic_section_memory = {};
  Addr base_addr = reinterpret_cast<Addr>(&dynamic_section_memory);

  dynamic_section_memory.dynamic_table_[0].d_tag = DT_INIT_ARRAYSZ;
  dynamic_section_memory.dynamic_table_[0].d_un.d_val = 2 * sizeof(Addr);
  dynamic_section_memory.dynamic_table_[1].d_tag = DT_FINI_ARRAYSZ;
  dynamic_section_memory.dynamic_table_[1].d_un.d_val = 2 * sizeof(Addr);

  DynamicSection d(base_addr, dynamic_section_memory.dynamic_table_, 2);
  ASSERT_TRUE(d.InitDynamicSection());
  d.CallConstructors();
  d.CallDestructors();
  EXPECT_FALSE(gCtor1Called);
  EXPECT_FALSE(gDtor1Called);
}

// When resolving symbols during Evergreen library loading or entry-point lookup
// for a library using a standard SysV DT_HASH table, DynamicSection must
// resolve defined global and weak symbols across hash chains while filtering
// out undefined, local, and absent symbols.
TEST_F(DynamicSectionTest, LookupByNameWithElfHash) {
  struct ElfHashMemory {
    Dyn dynamic_table[4];
    Word elf_hash[8];  // nbucket=1, nchain=5, bucket[1], chain[5]
    Sym symbol_table[5];
    char string_table[64];
  };
  ElfHashMemory mem = {};
  Addr base_addr = reinterpret_cast<Addr>(&mem);

  // String table: "\0elf_global\0elf_weak\0elf_undef\0elf_local\0"
  const char kStrings[] = "\0elf_global\0elf_weak\0elf_undef\0elf_local";
  std::memcpy(mem.string_table, kStrings, sizeof(kStrings));

  mem.symbol_table[1].st_name = 1;
  mem.symbol_table[1].st_info = static_cast<unsigned char>(STB_GLOBAL << 4);
  mem.symbol_table[1].st_shndx = 1;

  mem.symbol_table[2].st_name = 12;
  mem.symbol_table[2].st_info = static_cast<unsigned char>(STB_WEAK << 4);
  mem.symbol_table[2].st_shndx = 1;

  mem.symbol_table[3].st_name = 21;
  mem.symbol_table[3].st_info = static_cast<unsigned char>(STB_GLOBAL << 4);
  mem.symbol_table[3].st_shndx = SHN_UNDEF;

  mem.symbol_table[4].st_name = 31;
  mem.symbol_table[4].st_info = static_cast<unsigned char>(STB_LOCAL << 4);
  mem.symbol_table[4].st_shndx = 1;

  // 1 bucket -> chain 1 -> 2 -> 3 -> 4 -> 0
  mem.elf_hash[0] = 1;  // nbucket
  mem.elf_hash[1] = 5;  // nchain
  mem.elf_hash[2] = 1;  // bucket[0]
  mem.elf_hash[3] = 0;  // chain[0]
  mem.elf_hash[4] = 2;  // chain[1]
  mem.elf_hash[5] = 3;  // chain[2]
  mem.elf_hash[6] = 4;  // chain[3]
  mem.elf_hash[7] = 0;  // chain[4]

  mem.dynamic_table[0].d_tag = DT_SYMTAB;
  mem.dynamic_table[0].d_un.d_ptr =
      reinterpret_cast<Addr>(mem.symbol_table) - base_addr;
  mem.dynamic_table[1].d_tag = DT_STRTAB;
  mem.dynamic_table[1].d_un.d_ptr =
      reinterpret_cast<Addr>(mem.string_table) - base_addr;
  mem.dynamic_table[2].d_tag = DT_HASH;
  mem.dynamic_table[2].d_un.d_ptr =
      reinterpret_cast<Addr>(mem.elf_hash) - base_addr;
  mem.dynamic_table[3].d_tag = DT_NULL;

  DynamicSection d(base_addr, mem.dynamic_table, 4);
  ASSERT_TRUE(d.InitDynamicSection());

  EXPECT_EQ(&mem.symbol_table[1], d.LookupByName("elf_global"));
  EXPECT_EQ(&mem.symbol_table[2], d.LookupByName("elf_weak"));
  EXPECT_EQ(nullptr, d.LookupByName("elf_undef"));
  EXPECT_EQ(nullptr, d.LookupByName("elf_local"));
  EXPECT_EQ(nullptr, d.LookupByName("elf_missing"));
}

// When resolving symbols for a library using DT_GNU_HASH (the default format
// for modern Evergreen binaries), DynamicSection must check the bloom filter,
// locate the bucket, walk the hash chain to return defined global/weak symbols,
// and return null for undefined, local, empty-bucket, and missing symbols.
TEST_F(DynamicSectionTest, LookupByNameWithGnuHash) {
  struct alignas(Addr) GnuHashBlock {
    uint32_t num_buckets;
    uint32_t sym_offset;
    uint32_t bloom_size;
    uint32_t bloom_shift;
    Addr bloom[1];
    uint32_t buckets[2];
    uint32_t chain[4];
  };
  static_assert(offsetof(GnuHashBlock, bloom) == 4 * sizeof(uint32_t),
                "bloom filter must immediately follow the 4 header words");
  static_assert(
      offsetof(GnuHashBlock, buckets) == 4 * sizeof(uint32_t) + sizeof(Addr),
      "buckets must immediately follow the bloom filter");
  static_assert(offsetof(GnuHashBlock, chain) ==
                    4 * sizeof(uint32_t) + sizeof(Addr) + 2 * sizeof(uint32_t),
                "chains must immediately follow buckets");

  struct GnuHashMemory {
    Dyn dynamic_table[4];
    GnuHashBlock gnu_hash;
    Sym symbol_table[5];
    char string_table[64];
  };
  GnuHashMemory mem = {};
  Addr base_addr = reinterpret_cast<Addr>(&mem);

  // Names "sym_a", "sym_c", "sym_e", "sym_g", and "sym_i" all share the same
  // parity in ComputeGnuHash() so they map to the same bucket modulo 2, while
  // "sym_b" maps to the opposite (empty) bucket.
  const char kStrings[] = "\0sym_a\0sym_c\0sym_e\0sym_g";
  std::memcpy(mem.string_table, kStrings, sizeof(kStrings));

  mem.symbol_table[1].st_name = 1;  // sym_a: defined global
  mem.symbol_table[1].st_info = static_cast<unsigned char>(STB_GLOBAL << 4);
  mem.symbol_table[1].st_shndx = 1;

  mem.symbol_table[2].st_name = 7;  // sym_c: defined weak
  mem.symbol_table[2].st_info = static_cast<unsigned char>(STB_WEAK << 4);
  mem.symbol_table[2].st_shndx = 1;

  mem.symbol_table[3].st_name = 13;  // sym_e: undefined global
  mem.symbol_table[3].st_info = static_cast<unsigned char>(STB_GLOBAL << 4);
  mem.symbol_table[3].st_shndx = SHN_UNDEF;

  mem.symbol_table[4].st_name = 19;  // sym_g: defined local
  mem.symbol_table[4].st_info = static_cast<unsigned char>(STB_LOCAL << 4);
  mem.symbol_table[4].st_shndx = 1;

  const uint32_t h_a = ComputeGnuHash("sym_a");
  const uint32_t h_c = ComputeGnuHash("sym_c");
  const uint32_t h_e = ComputeGnuHash("sym_e");
  const uint32_t h_g = ComputeGnuHash("sym_g");
  const uint32_t h_b = ComputeGnuHash("sym_b");
  const uint32_t h_i = ComputeGnuHash("sym_i");
  const uint32_t populated_bucket = h_a % 2;
  ASSERT_EQ(populated_bucket, h_c % 2);
  ASSERT_EQ(populated_bucket, h_e % 2);
  ASSERT_EQ(populated_bucket, h_g % 2);
  ASSERT_EQ(populated_bucket, h_i % 2);
  ASSERT_NE(populated_bucket, h_b % 2);

  constexpr uint32_t kBloomShift = 6;
  auto add_to_bloom = [&](uint32_t h) {
    mem.gnu_hash.bloom[0] |= (Addr(1) << (h % ELF_BITS)) |
                             (Addr(1) << ((h >> kBloomShift) % ELF_BITS));
  };
  add_to_bloom(h_a);
  add_to_bloom(h_c);
  add_to_bloom(h_e);
  add_to_bloom(h_g);
  // Also set bits for sym_b and sym_i so they pass the bloom filter and
  // exercise empty-bucket and chain-termination miss paths respectively.
  add_to_bloom(h_b);
  add_to_bloom(h_i);

  mem.gnu_hash.num_buckets = 2;
  mem.gnu_hash.sym_offset = 1;
  mem.gnu_hash.bloom_size = 1;
  mem.gnu_hash.bloom_shift = kBloomShift;
  mem.gnu_hash.buckets[populated_bucket] = 1;
  mem.gnu_hash.buckets[1 - populated_bucket] = 0;
  mem.gnu_hash.chain[0] = h_a & ~1U;
  mem.gnu_hash.chain[1] = h_c & ~1U;
  mem.gnu_hash.chain[2] = h_e & ~1U;
  mem.gnu_hash.chain[3] = h_g | 1U;  // LSB marks end of chain.

  mem.dynamic_table[0].d_tag = DT_SYMTAB;
  mem.dynamic_table[0].d_un.d_ptr =
      reinterpret_cast<Addr>(mem.symbol_table) - base_addr;
  mem.dynamic_table[1].d_tag = DT_STRTAB;
  mem.dynamic_table[1].d_un.d_ptr =
      reinterpret_cast<Addr>(mem.string_table) - base_addr;
  mem.dynamic_table[2].d_tag = DT_GNU_HASH;
  mem.dynamic_table[2].d_un.d_ptr =
      reinterpret_cast<Addr>(&mem.gnu_hash) - base_addr;
  mem.dynamic_table[3].d_tag = DT_NULL;

  DynamicSection d(base_addr, mem.dynamic_table, 4);
  ASSERT_TRUE(d.InitDynamicSection());

  EXPECT_EQ(&mem.symbol_table[1], d.LookupByName("sym_a"));
  EXPECT_EQ(&mem.symbol_table[2], d.LookupByName("sym_c"));
  EXPECT_EQ(nullptr, d.LookupByName("sym_e"));
  EXPECT_EQ(nullptr, d.LookupByName("sym_g"));
  EXPECT_EQ(nullptr, d.LookupByName("sym_b"));  // Empty bucket.
  EXPECT_EQ(nullptr, d.LookupByName("sym_i"));  // Chain walk miss.
  EXPECT_EQ(nullptr, d.LookupByName("completely_absent_symbol"));

  // Clearing the bloom filter must short-circuit lookup even for present
  // symbols.
  mem.gnu_hash.bloom[0] = 0;
  EXPECT_EQ(nullptr, d.LookupByName("sym_a"));
}

// When an ELF binary lacks DT_HASH/DT_GNU_HASH or lacks DT_SYMTAB/DT_STRTAB,
// symbol lookup APIs must return null/false safely rather than dividing by zero
// or dereferencing null table pointers.
TEST_F(DynamicSectionTest, LookupWithoutHashOrSymbolTablesReturnsNull) {
  DynamicSectionMemory mem = {};
  Addr base_addr = reinterpret_cast<Addr>(&mem);

  // No DT_SYMTAB, DT_STRTAB, DT_HASH, or DT_GNU_HASH.
  DynamicSection empty_section(base_addr, mem.dynamic_table_, 1);
  ASSERT_TRUE(empty_section.InitDynamicSection());
  EXPECT_EQ(nullptr, empty_section.LookupById(0));
  EXPECT_FALSE(empty_section.IsWeakById(0));
  EXPECT_EQ(nullptr, empty_section.LookupNameById(0));
  EXPECT_EQ(nullptr, empty_section.LookupByName("SbLog"));
  EXPECT_EQ(nullptr, empty_section.LookupByName(nullptr));

  // DT_SYMTAB and DT_STRTAB present, but neither DT_HASH nor DT_GNU_HASH.
  mem.dynamic_table_[0].d_tag = DT_SYMTAB;
  mem.dynamic_table_[0].d_un.d_ptr =
      reinterpret_cast<Addr>(mem.symbol_table_) - base_addr;
  mem.dynamic_table_[1].d_tag = DT_STRTAB;
  mem.dynamic_table_[1].d_un.d_ptr =
      reinterpret_cast<Addr>(mem.string_table_) - base_addr;
  mem.dynamic_table_[2].d_tag = DT_NULL;

  DynamicSection no_hash_section(base_addr, mem.dynamic_table_, 3);
  ASSERT_TRUE(no_hash_section.InitDynamicSection());
  EXPECT_EQ(nullptr, no_hash_section.LookupByName("SbLog"));
  EXPECT_EQ(nullptr, no_hash_section.LookupByName(nullptr));
}

// When parsing a GNU hash table with zero bloom_size, non-power-of-two
// bloom_size, zero sym_offset, out-of-range bloom_shift, or all-zero buckets
// (including valid libraries exporting zero hashed symbols), GnuHashTable::Init
// must reject the table safely without OOB reads or unsigned chain-index
// underflow, and DynamicSection::LookupByName must return null without
// crashing.
TEST_F(DynamicSectionTest, GnuHashTableRejectsMalformedOrEmptyTables) {
  {
    GnuHashTable table;
    table.Init(0);
    EXPECT_FALSE(table.IsValid());
    EXPECT_EQ(0u, table.dyn_symbols_offset());
    EXPECT_EQ(0u, table.dyn_symbols_count());
  }

  // Zero bloom_size must not pass the power-of-two check. With bloom_size == 0,
  // buckets[0] would read word 4 and chain[0] word 5; populating valid non-zero
  // entries there ensures rejection comes from the bloom_size == 0 check.
  {
    alignas(Addr) uint32_t raw[6] = {1, 1, 0, 6, 1, 1};
    GnuHashTable table;
    table.Init(reinterpret_cast<uintptr_t>(raw));
    EXPECT_FALSE(table.IsValid());
    EXPECT_EQ(0u, table.dyn_symbols_offset());
  }

  // Non-power-of-two bloom_size must be rejected even when valid bucket and
  // chain entries sit immediately after the 3-word bloom filter.
  {
    struct alignas(Addr) NonPow2BloomGnuHash {
      uint32_t num_buckets;
      uint32_t sym_offset;
      uint32_t bloom_size;
      uint32_t bloom_shift;
      Addr bloom[3];
      uint32_t buckets[1];
      uint32_t chain[1];
    };
    static_assert(offsetof(NonPow2BloomGnuHash, buckets) ==
                      4 * sizeof(uint32_t) + 3 * sizeof(Addr),
                  "buckets must immediately follow the 3-word bloom filter");
    static_assert(
        offsetof(NonPow2BloomGnuHash, chain) ==
            4 * sizeof(uint32_t) + 3 * sizeof(Addr) + sizeof(uint32_t),
        "chain must immediately follow buckets");
    NonPow2BloomGnuHash raw = {};
    raw.num_buckets = 1;
    raw.sym_offset = 1;
    raw.bloom_size = 3;
    raw.bloom_shift = 6;
    raw.buckets[0] = 1;
    raw.chain[0] = 1;

    GnuHashTable table;
    table.Init(reinterpret_cast<uintptr_t>(&raw));
    EXPECT_FALSE(table.IsValid());
    EXPECT_EQ(0u, table.dyn_symbols_offset());
  }

  // Verify single-entry layout baseline (bloom_shift == 31 valid), then verify
  // bloom_shift == 32 (shift UB on 32-bit hashes) and sym_offset == 0 are
  // rejected.
  {
    struct alignas(Addr) SingleEntryGnuHash {
      uint32_t num_buckets;
      uint32_t sym_offset;
      uint32_t bloom_size;
      uint32_t bloom_shift;
      Addr bloom[1];
      uint32_t buckets[1];
      uint32_t chain[1];
    };
    static_assert(offsetof(SingleEntryGnuHash, buckets) ==
                      4 * sizeof(uint32_t) + sizeof(Addr),
                  "buckets must immediately follow bloom");
    static_assert(offsetof(SingleEntryGnuHash, chain) ==
                      4 * sizeof(uint32_t) + sizeof(Addr) + sizeof(uint32_t),
                  "chain must immediately follow buckets");
    SingleEntryGnuHash raw = {};
    raw.num_buckets = 1;
    raw.sym_offset = 1;
    raw.bloom_size = 1;
    raw.bloom_shift = 31;
    raw.buckets[0] = 1;
    raw.chain[0] = 1;

    GnuHashTable valid_table;
    valid_table.Init(reinterpret_cast<uintptr_t>(&raw));
    ASSERT_TRUE(valid_table.IsValid());
    EXPECT_EQ(1u, valid_table.dyn_symbols_count());

    raw.bloom_shift = 32;
    GnuHashTable bad_shift_table;
    bad_shift_table.Init(reinterpret_cast<uintptr_t>(&raw));
    EXPECT_FALSE(bad_shift_table.IsValid());
    EXPECT_EQ(0u, bad_shift_table.dyn_symbols_offset());

    raw.bloom_shift = 6;
    raw.sym_offset = 0;
    GnuHashTable bad_offset_table;
    bad_offset_table.Init(reinterpret_cast<uintptr_t>(&raw));
    EXPECT_FALSE(bad_offset_table.IsValid());
  }

  // All-zero buckets with sym_offset > 0 must not underflow chain_ index.
  {
    struct alignas(Addr) EmptyBucketsGnuHash {
      uint32_t num_buckets;
      uint32_t sym_offset;
      uint32_t bloom_size;
      uint32_t bloom_shift;
      Addr bloom[1];
      uint32_t buckets[2];
      uint32_t chain[1];
    };
    static_assert(offsetof(EmptyBucketsGnuHash, buckets) ==
                      4 * sizeof(uint32_t) + sizeof(Addr),
                  "buckets must immediately follow bloom");
    static_assert(
        offsetof(EmptyBucketsGnuHash, chain) ==
            4 * sizeof(uint32_t) + sizeof(Addr) + 2 * sizeof(uint32_t),
        "chain must immediately follow buckets");
    struct EmptyExportMemory {
      Dyn dynamic_table[4];
      EmptyBucketsGnuHash gnu_hash;
      Sym symbol_table[2];
      char string_table[16];
    };
    EmptyExportMemory mem = {};
    Addr base_addr = reinterpret_cast<Addr>(&mem);
    mem.gnu_hash.num_buckets = 2;
    mem.gnu_hash.sym_offset = 1;
    mem.gnu_hash.bloom_size = 1;
    mem.gnu_hash.bloom_shift = 6;
    // buckets[0] == 0 and buckets[1] == 0 => max_index (0) < sym_offset (1).

    GnuHashTable table;
    table.Init(reinterpret_cast<uintptr_t>(&mem.gnu_hash));
    EXPECT_FALSE(table.IsValid());
    EXPECT_EQ(0u, table.dyn_symbols_offset());

    mem.dynamic_table[0].d_tag = DT_SYMTAB;
    mem.dynamic_table[0].d_un.d_ptr =
        reinterpret_cast<Addr>(mem.symbol_table) - base_addr;
    mem.dynamic_table[1].d_tag = DT_STRTAB;
    mem.dynamic_table[1].d_un.d_ptr =
        reinterpret_cast<Addr>(mem.string_table) - base_addr;
    mem.dynamic_table[2].d_tag = DT_GNU_HASH;
    mem.dynamic_table[2].d_un.d_ptr =
        reinterpret_cast<Addr>(&mem.gnu_hash) - base_addr;
    mem.dynamic_table[3].d_tag = DT_NULL;

    DynamicSection d(base_addr, mem.dynamic_table, 4);
    ASSERT_TRUE(d.InitDynamicSection());
    EXPECT_EQ(nullptr, d.LookupByName("any_symbol"));
  }
}

// When parsing a malformed SysV DT_HASH table (null address, zero bucket/chain
// counts, out-of-range bucket index, or cyclic hash chain), ElfHashTable must
// avoid underflow, OOB reads, and infinite loops.
TEST_F(DynamicSectionTest, ElfHashTableRejectsMalformedTablesAndCycles) {
  Sym symbol_table[3] = {};
  const char string_table[] = "\0foo\0bar";
  symbol_table[1].st_name = 1;
  symbol_table[2].st_name = 5;

  {
    ElfHashTable table;
    table.Init(0);
    EXPECT_FALSE(table.IsValid());
    EXPECT_EQ(0u, table.dyn_symbols_count());
    EXPECT_EQ(nullptr, table.LookupByName("foo", symbol_table, string_table));
  }

  // Zero nbucket must keep dyn_symbols_count() at 0 even when nchain > 0.
  {
    Word raw[3] = {0, 5, 0};  // nbucket=0, nchain=5
    ElfHashTable table;
    table.Init(reinterpret_cast<uintptr_t>(raw));
    EXPECT_FALSE(table.IsValid());
    EXPECT_EQ(0u, table.dyn_symbols_count());
    EXPECT_EQ(nullptr, table.LookupByName("foo", symbol_table, string_table));
  }

  // Zero nchain must be rejected and dyn_symbols_count() must not underflow.
  {
    Word raw[3] = {1, 0, 0};  // nbucket=1, nchain=0
    ElfHashTable table;
    table.Init(reinterpret_cast<uintptr_t>(raw));
    EXPECT_FALSE(table.IsValid());
    EXPECT_EQ(0u, table.dyn_symbols_count());
    EXPECT_EQ(nullptr, table.LookupByName("foo", symbol_table, string_table));
  }

  // Out-of-range bucket index (>= nchain) must stop without reading OOB.
  {
    Word raw[6] = {1, 3, 3, 0, 0, 0};  // bucket[0] == 3 == nchain
    ElfHashTable table;
    table.Init(reinterpret_cast<uintptr_t>(raw));
    ASSERT_TRUE(table.IsValid());
    EXPECT_EQ(nullptr,
              table.LookupByName("missing", symbol_table, string_table));
  }

  // Cyclic chain (1 -> 2 -> 1) must terminate instead of hanging.
  {
    Word raw[6] = {1, 3, 1, 0, 2, 1};
    ElfHashTable table;
    table.Init(reinterpret_cast<uintptr_t>(raw));
    ASSERT_TRUE(table.IsValid());
    EXPECT_EQ(nullptr,
              table.LookupByName("missing", symbol_table, string_table));
  }
}

}  // namespace
}  // namespace elf_loader
