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

#include "starboard/common/scoped_ptr.h"
#include "starboard/elf_loader/elf.h"
#include "starboard/elf_loader/file_impl.h"
#include "starboard/string.h"
#include "testing/gtest/include/gtest/gtest.h"

#if SB_API_VERSION >= 12 && (SB_API_VERSION >= 12 || SB_HAS(MMAP)) && \
    SB_CAN(MAP_EXECUTABLE_MEMORY)
namespace starboard {
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
  scoped_ptr<Relocations> relocations_;
  Addr base_addr_;

 private:
  char buf_[128];
  Dyn dynamic_table_[10];
  scoped_ptr<DynamicSection> dynamic_section_;
  scoped_ptr<ExportedSymbols> exported_symbols_;
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

}  // namespace
}  // namespace elf_loader
}  // namespace starboard
#endif  // SB_API_VERSION >= 12 && (SB_API_VERSION >= 12
        // || SB_HAS(MMAP)) && SB_CAN(MAP_EXECUTABLE_MEMORY)
