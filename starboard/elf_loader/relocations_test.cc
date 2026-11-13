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

#include "starboard/elf_loader/relocations.h"

#include <sys/mman.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>
#include <memory>

#include "starboard/elf_loader/elf.h"
#include "starboard/elf_loader/file_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace elf_loader {

namespace {

// Test constants used as sample data.
const Addr kTestAddress = 34;
const Sword kTestAddend = 5;

class RelocationsTest : public ::testing::Test {
 protected:
  RelocationsTest() {
    memset(buf_, 'A', sizeof(buf_));
    base_addr_ = reinterpret_cast<Addr>(&buf_);
    dynamic_table_[0].d_tag = DT_REL;

    dynamic_section_.reset(
        new DynamicSection(base_addr_, dynamic_table_, 1, 0));
    dynamic_section_->InitDynamicSection();

    exported_symbols_.reset(new ExportedSymbols());
    relocations_.reset(new Relocations(base_addr_, dynamic_section_.get(),
                                       exported_symbols_.get()));
  }
  ~RelocationsTest() {}

  void VerifySymAddress(const rel_t* rel, Addr sym_addr) {
    Addr target_addr = base_addr_ + rel->r_offset;
    Addr target_value = sym_addr;
    EXPECT_EQ(target_value, *reinterpret_cast<Addr*>(target_addr));
  }

#ifdef USE_RELA
  void VerifySymAddressPlusAddend(const rel_t* rel, Addr sym_addr) {
    Addr target_addr = base_addr_ + rel->r_offset;
    Addr target_value = sym_addr + rel->r_addend;
    EXPECT_EQ(target_value, *reinterpret_cast<Addr*>(target_addr));
  }

  void VerifyBaseAddressPlusAddend(const rel_t* rel) {
    Addr target_addr = base_addr_ + rel->r_offset;
    Addr target_value = base_addr_ + rel->r_addend;
    EXPECT_EQ(target_value, *reinterpret_cast<Addr*>(target_addr));
  }

  void VerifySymAddressPlusAddendDelta(const rel_t* rel, Addr sym_addr) {
    Addr target_addr = base_addr_ + rel->r_offset;
    Addr offset_rel = rel->r_offset + base_addr_;
    Addr target_value = sym_addr + (rel->r_addend - offset_rel);
    EXPECT_EQ(target_value, *reinterpret_cast<Addr*>(target_addr));
  }
#endif

 protected:
  std::unique_ptr<Relocations> relocations_;
  Addr base_addr_;

 private:
  char buf_[128];
  Dyn dynamic_table_[10];
  std::unique_ptr<DynamicSection> dynamic_section_;
  std::unique_ptr<ExportedSymbols> exported_symbols_;
};

#if SB_IS(ARCH_ARM)
TEST_F(RelocationsTest, R_ARM_JUMP_SLOT) {
  rel_t rel;
  rel.r_offset = 0;
  rel.r_info = R_ARM_JUMP_SLOT;
  Addr sym_addr = kTestAddress;

  // Expected relocation calculation:
  //   *target = sym_addr;
  relocations_->ApplyResolvedReloc(&rel, sym_addr);

  VerifySymAddress(&rel, sym_addr);
}

TEST_F(RelocationsTest, R_ARM_GLOB_DAT) {
  rel_t rel;
  rel.r_offset = 1;
  rel.r_info = R_ARM_GLOB_DAT;
  Addr sym_addr = kTestAddress;

  // Expected relocation calculation:
  //   *target = sym_addr;
  relocations_->ApplyResolvedReloc(&rel, sym_addr);

  VerifySymAddress(&rel, sym_addr);
}

TEST_F(RelocationsTest, R_ARM_ABS32) {
  rel_t rel;
  rel.r_offset = 2;
  rel.r_info = R_ARM_ABS32;
  Addr sym_addr = kTestAddress;

  Addr target_addr = base_addr_ + rel.r_offset;
  Addr target_value = *reinterpret_cast<Addr*>(target_addr);
  target_value += sym_addr;

  // Expected relocation calculation:
  //   *target += sym_addr;
  relocations_->ApplyResolvedReloc(&rel, sym_addr);

  EXPECT_EQ(target_value, *reinterpret_cast<Addr*>(target_addr));
}

TEST_F(RelocationsTest, R_ARM_REL32) {
  rel_t rel;
  rel.r_offset = 3;
  rel.r_info = R_ARM_REL32;
  Addr sym_addr = kTestAddress;

  Addr target_addr = base_addr_ + rel.r_offset;
  Addr target_value = *reinterpret_cast<Addr*>(target_addr);
  target_value += sym_addr - rel.r_offset;

  // Expected relocation calculation:
  //   *target += sym_addr - rel->r_offset;
  relocations_->ApplyResolvedReloc(&rel, sym_addr);

  EXPECT_EQ(target_value, *reinterpret_cast<Addr*>(target_addr));
}

TEST_F(RelocationsTest, R_ARM_RELATIVE) {
  rel_t rel;
  rel.r_offset = 4;
  rel.r_info = R_ARM_RELATIVE;
  Addr sym_addr = kTestAddress;

  Addr target_addr = base_addr_ + rel.r_offset;
  Addr target_value = *reinterpret_cast<Addr*>(target_addr);
  target_value += base_addr_;

  // Expected relocation calculation:
  //   *target += base_memory_address_;
  relocations_->ApplyResolvedReloc(&rel, sym_addr);

  EXPECT_EQ(target_value, *reinterpret_cast<Addr*>(target_addr));
}
#endif  // SB_IS(ARCH_ARM)

#if SB_IS(ARCH_ARM64) && defined(USE_RELA)
TEST_F(RelocationsTest, R_AARCH64_JUMP_SLOT) {
  rel_t rel;
  rel.r_offset = 0;
  rel.r_info = R_AARCH64_JUMP_SLOT;
  rel.r_addend = kTestAddend;
  Addr sym_addr = kTestAddress;

  // Expected relocation calculation:
  //   *target = sym_addr + addend;
  relocations_->ApplyResolvedReloc(&rel, sym_addr);

  VerifySymAddressPlusAddend(&rel, sym_addr);
}

TEST_F(RelocationsTest, R_AARCH64_GLOB_DAT) {
  rel_t rel;
  rel.r_offset = 1;
  rel.r_info = R_AARCH64_GLOB_DAT;
  rel.r_addend = kTestAddend;
  Addr sym_addr = kTestAddress;

  // Expected relocation calculation:
  //   *target = sym_addr + addend;
  relocations_->ApplyResolvedReloc(&rel, sym_addr);

  VerifySymAddressPlusAddend(&rel, sym_addr);
}

TEST_F(RelocationsTest, R_AARCH64_ABS64) {
  rel_t rel;
  rel.r_offset = 2;
  rel.r_info = R_AARCH64_ABS64;
  rel.r_addend = kTestAddend;
  Addr sym_addr = kTestAddress;

  Addr target_addr = base_addr_ + rel.r_offset;
  Addr target_value = *reinterpret_cast<Addr*>(target_addr);
  target_value += sym_addr + rel.r_addend;

  // Expected relocation calculation:
  //   *target += sym_addr + addend;
  relocations_->ApplyResolvedReloc(&rel, sym_addr);

  EXPECT_EQ(target_value, *reinterpret_cast<Addr*>(target_addr));
}

TEST_F(RelocationsTest, R_AARCH64_RELATIVE) {
  rel_t rel;
  rel.r_offset = 3;
  rel.r_info = R_AARCH64_RELATIVE;
  rel.r_addend = kTestAddend;
  Addr sym_addr = kTestAddress;

  // Expected relocation calculation:
  //   *target = base_memory_address_ + addend;
  relocations_->ApplyResolvedReloc(&rel, sym_addr);

  VerifyBaseAddressPlusAddend(&rel);
}

#endif  // SB_IS(ARCH_ARM64) && defined(USE_RELA)

#if SB_IS(ARCH_X86)
TEST_F(RelocationsTest, R_386_JMP_SLOT) {
  rel_t rel;
  rel.r_offset = 0;
  rel.r_info = R_386_JMP_SLOT;
  Addr sym_addr = kTestAddress;

  // Expected relocation calculation:
  //   *target = sym_addr;
  relocations_->ApplyResolvedReloc(&rel, sym_addr);

  VerifySymAddress(&rel, sym_addr);
}

TEST_F(RelocationsTest, R_386_GLOB_DAT) {
  rel_t rel;
  rel.r_offset = 1;
  rel.r_info = R_386_GLOB_DAT;
  Addr sym_addr = kTestAddress;

  // Expected relocation calculation:
  //   *target = sym_addr;
  relocations_->ApplyResolvedReloc(&rel, sym_addr);

  VerifySymAddress(&rel, sym_addr);
}

TEST_F(RelocationsTest, R_386_RELATIVE) {
  rel_t rel;
  rel.r_offset = 2;
  rel.r_info = R_386_RELATIVE;
  Addr sym_addr = kTestAddress;

  Addr target_addr = base_addr_ + rel.r_offset;
  Addr target_value = *reinterpret_cast<Addr*>(target_addr);
  target_value += base_addr_;

  // Expected relocation calculation:
  //   *target += base_memory_address_;
  relocations_->ApplyResolvedReloc(&rel, sym_addr);

  EXPECT_EQ(target_value, *reinterpret_cast<Addr*>(target_addr));
}

TEST_F(RelocationsTest, R_386_32) {
  rel_t rel;
  rel.r_offset = 3;
  rel.r_info = R_386_32;
  Addr sym_addr = kTestAddress;

  Addr target_addr = base_addr_ + rel.r_offset;
  Addr target_value = *reinterpret_cast<Addr*>(target_addr);
  target_value += sym_addr;

  // Expected relocation calculation:
  //   *target += sym_addr;
  relocations_->ApplyResolvedReloc(&rel, sym_addr);

  EXPECT_EQ(target_value, *reinterpret_cast<Addr*>(target_addr));
}

TEST_F(RelocationsTest, R_386_PC32) {
  rel_t rel;
  rel.r_offset = 4;
  rel.r_info = R_386_PC32;
  Addr sym_addr = kTestAddress;

  Addr target_addr = base_addr_ + rel.r_offset;
  Addr target_value = *reinterpret_cast<Addr*>(target_addr);
  Addr reloc = static_cast<Addr>(rel.r_offset + base_addr_);
  target_value += (sym_addr - reloc);

  // Expected relocation calculation:
  //   *target += (sym_addr - reloc);
  relocations_->ApplyResolvedReloc(&rel, sym_addr);

  EXPECT_EQ(target_value, *reinterpret_cast<Addr*>(target_addr));
}
#endif  // SB_IS(ARCH_X86)

#if SB_IS(ARCH_X64) && defined(USE_RELA)
TEST_F(RelocationsTest, R_X86_64_JMP_SLOT) {
  rel_t rel;
  rel.r_offset = 0;
  rel.r_info = R_X86_64_JMP_SLOT;
  rel.r_addend = kTestAddend;
  Addr sym_addr = kTestAddress;

  // Expected relocation calculation:
  //   *target = sym_addr + addend;
  relocations_->ApplyResolvedReloc(&rel, sym_addr);

  VerifySymAddressPlusAddend(&rel, sym_addr);
}

TEST_F(RelocationsTest, R_X86_64_GLOB_DAT) {
  rel_t rel;
  rel.r_offset = 1;
  rel.r_info = R_X86_64_GLOB_DAT;
  rel.r_addend = kTestAddend;
  Addr sym_addr = kTestAddress;

  // Expected relocation calculation:
  //   *target = sym_addr + addend;
  relocations_->ApplyResolvedReloc(&rel, sym_addr);

  VerifySymAddressPlusAddend(&rel, sym_addr);
}

TEST_F(RelocationsTest, R_X86_64_RELATIVE) {
  rel_t rel;
  rel.r_offset = 2;
  rel.r_info = R_X86_64_RELATIVE;
  rel.r_addend = kTestAddend;
  Addr sym_addr = kTestAddress;

  // Expected relocation calculation:
  //   *target = base_memory_address_ + addend;
  relocations_->ApplyResolvedReloc(&rel, sym_addr);

  VerifyBaseAddressPlusAddend(&rel);
}

TEST_F(RelocationsTest, R_X86_64_64) {
  rel_t rel;
  rel.r_offset = 3;
  rel.r_info = R_X86_64_64;
  rel.r_addend = kTestAddend;
  Addr sym_addr = kTestAddress;

  // Expected relocation calculation:
  //   *target = sym_addr + addend;
  relocations_->ApplyResolvedReloc(&rel, sym_addr);

  VerifySymAddressPlusAddend(&rel, sym_addr);
}

TEST_F(RelocationsTest, R_X86_64_PC32) {
  rel_t rel;
  rel.r_offset = 4;
  rel.r_info = R_X86_64_PC32;
  rel.r_addend = kTestAddend;
  Addr sym_addr = kTestAddress;

  relocations_->ApplyResolvedReloc(&rel, sym_addr);

  // Expected relocation calculation:
  //   *target = sym_addr + (addend - reloc);
  VerifySymAddressPlusAddendDelta(&rel, sym_addr);
}
#endif  // SB_IS(ARCH_X64) && defined(USE_RELA)

TEST_F(RelocationsTest, ApplyAllRelocationsReclaimsInteriorPages) {
  long sys_page_size = sysconf(_SC_PAGESIZE);  // NOLINT(runtime/int)
  const size_t page_size = (sys_page_size > static_cast<long>(PAGE_SIZE) &&
                            (sys_page_size & (sys_page_size - 1)) == 0)
                               ? static_cast<size_t>(sys_page_size)
                               : PAGE_SIZE;
  const size_t kMappingSize = 4 * page_size;
  void* mapped = mmap(nullptr, kMappingSize, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  ASSERT_NE(mapped, MAP_FAILED);
  std::memset(mapped, 0x5A, kMappingSize);

  Addr mapped_base = reinterpret_cast<Addr>(mapped);
  // Start the contiguous relocation tables (.rel(a).dyn followed immediately by
  // .rel(a).plt) 64 bytes into page 1, split them in the middle of page 2, and
  // end in page 3. Neither table covers a full page on its own, so page 2 is
  // only reclaimed when Relocations::DiscardRelocationPages() merges contiguous
  // DT_REL(A) and DT_JMPREL ranges, while the shared boundary pages (page 1 and
  // page 3) remain untouched.
  Addr reloc_table_offset = page_size + 64;
  size_t reloc_count = (2 * page_size) / sizeof(rel_t);
  size_t dyn_reloc_count = (page_size + page_size / 2 - 64) / sizeof(rel_t);
  size_t plt_reloc_count = reloc_count - dyn_reloc_count;
  size_t dyn_reloc_size = dyn_reloc_count * sizeof(rel_t);
  size_t plt_reloc_size = plt_reloc_count * sizeof(rel_t);
  Addr jmp_rel_offset = reloc_table_offset + dyn_reloc_size;

  // Populate every relocation entry with r_info = 0 (R_*_NONE, a no-op
  // relocation) and non-zero r_offset (plus r_addend on RELA) so that every
  // sizeof(rel_t) slice on the interior page contains non-zero bytes before
  // ApplyAllRelocations() across 4 KiB, 16 KiB, and 64 KiB page sizes, and is
  // zeroed by MADV_DONTNEED after.
  rel_t* reloc_entries =
      reinterpret_cast<rel_t*>(mapped_base + reloc_table_offset);
  for (size_t i = 0; i < reloc_count; ++i) {
    std::memset(&reloc_entries[i], 0, sizeof(rel_t));
    reloc_entries[i].r_offset = 0x7F7F7F7Fu;
#if defined(USE_RELA)
    reloc_entries[i].r_addend = 0x7F7F7F7F;
#endif
  }

  const uint8_t* interior_page =
      reinterpret_cast<const uint8_t*>(mapped_base + 2 * page_size);
  ASSERT_TRUE(std::any_of(interior_page, interior_page + sizeof(rel_t),
                          [](uint8_t b) { return b != 0u; }));
  ASSERT_TRUE(std::any_of(interior_page + page_size / 2,
                          interior_page + page_size / 2 + sizeof(rel_t),
                          [](uint8_t b) { return b != 0u; }));

  Dyn dyn_entries[6];
  std::memset(dyn_entries, 0, sizeof(dyn_entries));
#if defined(USE_RELA)
  dyn_entries[0].d_tag = DT_RELA;
  dyn_entries[1].d_tag = DT_RELASZ;
  dyn_entries[2].d_tag = DT_PLTREL;
  dyn_entries[2].d_un.d_val = DT_RELA;
#else
  dyn_entries[0].d_tag = DT_REL;
  dyn_entries[1].d_tag = DT_RELSZ;
  dyn_entries[2].d_tag = DT_PLTREL;
  dyn_entries[2].d_un.d_val = DT_REL;
#endif
  dyn_entries[0].d_un.d_ptr = reloc_table_offset;
  dyn_entries[1].d_un.d_val = dyn_reloc_size;
  dyn_entries[3].d_tag = DT_JMPREL;
  dyn_entries[3].d_un.d_ptr = jmp_rel_offset;
  dyn_entries[4].d_tag = DT_PLTRELSZ;
  dyn_entries[4].d_un.d_val = plt_reloc_size;
  dyn_entries[5].d_tag = DT_NULL;

  DynamicSection dyn_section(mapped_base, dyn_entries, 6, 0);
  ASSERT_TRUE(dyn_section.InitDynamicSection());

  ExportedSymbols symbols;
  Relocations relocs(mapped_base, &dyn_section, &symbols);
  ASSERT_TRUE(relocs.InitRelocations());
  ASSERT_TRUE(relocs.ApplyAllRelocations());

  // ApplyAllRelocations() alone leaves the relocation pages intact until
  // DiscardRelocationPages() is called (after peak load memory is sampled).
  EXPECT_TRUE(std::any_of(interior_page, interior_page + sizeof(rel_t),
                          [](uint8_t b) { return b != 0u; }));

  relocs.DiscardRelocationPages();

  // Every byte of interior page 2 of the relocation table should be zeroed by
  // MADV_DONTNEED.
  EXPECT_TRUE(std::all_of(interior_page, interior_page + page_size,
                          [](uint8_t b) { return b == 0u; }));

  // Shared boundary bytes in page 1 (before relocation table) and page 3
  // (after relocation table) must not be discarded.
  const uint8_t* page_1 =
      reinterpret_cast<const uint8_t*>(mapped_base + page_size);
  const uint8_t* page_3 =
      reinterpret_cast<const uint8_t*>(mapped_base + 3 * page_size);
  EXPECT_EQ(page_1[0], 0x5Au);
  EXPECT_EQ(page_3[128], 0x5Au);

  munmap(mapped, kMappingSize);
}

#if SB_IS(ARCH_ARM)
constexpr Word kGlobDatRelocType = R_ARM_GLOB_DAT;
#elif SB_IS(ARCH_ARM64)
constexpr Word kGlobDatRelocType = R_AARCH64_GLOB_DAT;
#elif SB_IS(ARCH_X86)
constexpr Word kGlobDatRelocType = R_386_GLOB_DAT;
#elif SB_IS(ARCH_X64)
constexpr Word kGlobDatRelocType = R_X86_64_GLOB_DAT;
#endif

#if SB_SIZE_OF(POINTER) == 4
Elf32_Word MakeRelocInfo(Elf32_Word sym, Elf32_Word type) {
  return ELF32_R_INFO(sym, type);
}
#else
Elf64_Xword MakeRelocInfo(Elf64_Word sym, Elf64_Word type) {
  return ELF64_R_INFO(sym, type);
}
#endif

struct WeakSymbolRelocationMemory {
  Addr got_slots[2];
  Word elf_hash[6];
  Sym symbol_table[3];
  char string_table[32];
  rel_t relocs[2];
  Dyn dynamic_table[6];
};

// When elf_loader loads an Evergreen guest binary (such as libcobalt.so during
// app launch or browser tests), ExportedSymbols::Lookup resolves Starboard ABI
// symbols and handles unregistered references according to ELF binding:
// optional weak runtime hooks (such as __cxa_thread_atexit_impl during
// thread_local initialization) must resolve to nullptr rather than binding host
// libc symbols via dlsym, whereas strong unregistered symbols (such as ASan
// runtime symbols in non-official builds) fall back to host dlsym.
TEST_F(RelocationsTest, LookupSkipsHostDlsymOnlyForUnregisteredWeakSymbols) {
  const ExportedSymbols symbols;
  EXPECT_NE(nullptr, symbols.Lookup("SbLog", /*is_weak=*/false));
  EXPECT_NE(nullptr, symbols.Lookup("SbLog", /*is_weak=*/true));
  EXPECT_EQ(nullptr, symbols.Lookup(nullptr, /*is_weak=*/false));
  EXPECT_EQ(nullptr, symbols.Lookup(nullptr, /*is_weak=*/true));
  EXPECT_EQ(nullptr,
            symbols.Lookup("__cxa_thread_atexit_impl", /*is_weak=*/true));
  EXPECT_EQ(nullptr, symbols.Lookup("dlsym", /*is_weak=*/true));
#if !defined(OFFICIAL_BUILD)
  EXPECT_NE(nullptr, symbols.Lookup("dlsym", /*is_weak=*/false));
#else
  EXPECT_EQ(nullptr, symbols.Lookup("dlsym", /*is_weak=*/false));
#endif
}

// End-to-end relocation journey: when an Evergreen guest module contains both
// an undefined STB_WEAK reference to an unregistered host symbol ("dlsym") and
// an undefined STB_WEAK reference to a registered Starboard API ("SbLog"),
// Relocations::ApplyAllRelocations resolves the unregistered weak symbol to 0
// while resolving the registered Starboard symbol to its exported address, and
// still resolves STB_GLOBAL unregistered symbols via dlsym in non-official
// builds.
TEST_F(RelocationsTest, ApplyAllRelocationsSkipsHostDlsymForWeakUndefined) {
  WeakSymbolRelocationMemory mem;
  std::memset(&mem, 0, sizeof(mem));
  const Addr base = reinterpret_cast<Addr>(&mem);

  mem.got_slots[0] = 0x1234u;
  mem.got_slots[1] = 0u;

  // Empty 1-bucket ELF hash table so DynamicSection::LookupByName reports
  // neither symbol as defined inside the guest module.
  mem.elf_hash[0] = 1;
  mem.elf_hash[1] = 3;

  std::memcpy(mem.string_table, "\0dlsym\0SbLog\0", 13);

  // Symbol 1: undefined STB_WEAK "dlsym" (unregistered host symbol).
  mem.symbol_table[1].st_name = 1;
  mem.symbol_table[1].st_info = static_cast<unsigned char>(STB_WEAK << 4);
  mem.symbol_table[1].st_shndx = SHN_UNDEF;

  // Symbol 2: undefined STB_WEAK "SbLog" (registered Starboard symbol).
  mem.symbol_table[2].st_name = 7;
  mem.symbol_table[2].st_info = static_cast<unsigned char>(STB_WEAK << 4);
  mem.symbol_table[2].st_shndx = SHN_UNDEF;

  mem.relocs[0].r_offset = reinterpret_cast<Addr>(&mem.got_slots[0]) - base;
  mem.relocs[0].r_info = MakeRelocInfo(1, kGlobDatRelocType);
  mem.relocs[1].r_offset = reinterpret_cast<Addr>(&mem.got_slots[1]) - base;
  mem.relocs[1].r_info = MakeRelocInfo(2, kGlobDatRelocType);

  mem.dynamic_table[0].d_tag = DT_HASH;
  mem.dynamic_table[0].d_un.d_ptr = reinterpret_cast<Addr>(mem.elf_hash) - base;
  mem.dynamic_table[1].d_tag = DT_STRTAB;
  mem.dynamic_table[1].d_un.d_ptr =
      reinterpret_cast<Addr>(mem.string_table) - base;
  mem.dynamic_table[2].d_tag = DT_SYMTAB;
  mem.dynamic_table[2].d_un.d_ptr =
      reinterpret_cast<Addr>(mem.symbol_table) - base;
#if defined(USE_RELA)
  mem.dynamic_table[3].d_tag = DT_RELA;
  mem.dynamic_table[4].d_tag = DT_RELASZ;
#else
  mem.dynamic_table[3].d_tag = DT_REL;
  mem.dynamic_table[4].d_tag = DT_RELSZ;
#endif
  mem.dynamic_table[3].d_un.d_ptr = reinterpret_cast<Addr>(mem.relocs) - base;
  mem.dynamic_table[4].d_un.d_val = sizeof(mem.relocs);
  mem.dynamic_table[5].d_tag = DT_NULL;

  DynamicSection dyn_section(base, mem.dynamic_table, 6, 0);
  ASSERT_TRUE(dyn_section.InitDynamicSection());

  ExportedSymbols symbols;
  Relocations relocs(base, &dyn_section, &symbols);
  ASSERT_TRUE(relocs.InitRelocations());
  ASSERT_TRUE(relocs.ApplyAllRelocations());

  EXPECT_EQ(0u, mem.got_slots[0]);
  EXPECT_EQ(reinterpret_cast<Addr>(symbols.Lookup("SbLog")), mem.got_slots[1]);

  // Flip "dlsym" to STB_GLOBAL to verify strong symbol resolution behavior.
  mem.symbol_table[1].st_info = static_cast<unsigned char>(STB_GLOBAL << 4);
#if !defined(OFFICIAL_BUILD)
  ASSERT_TRUE(relocs.ApplyAllRelocations());
  EXPECT_EQ(reinterpret_cast<Addr>(symbols.Lookup("dlsym", /*is_weak=*/false)),
            mem.got_slots[0]);
#else
  EXPECT_FALSE(relocs.ApplyAllRelocations());
#endif
}

}  // namespace
}  // namespace elf_loader
